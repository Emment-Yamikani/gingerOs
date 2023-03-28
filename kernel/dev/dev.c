#include <bits/errno.h>
#include <dev/dev.h>
#include <dev/ramdisk.h>
#include <dev/uart.h>
#include <dev/cons.h>
#include <dev/kbd.h>
#include <fs/fs.h>
#include <mm/kalloc.h>
#include <lib/string.h>
#include <locks/spinlock.h>
#include <printk.h>
#include <sys/system.h>
#include <lime/module.h>
#include <dev/bio.h>

static dev_t **blkdevs = NULL;
static spinlock_t *blkdevs_lock = SPINLOCK_NEW("blkdevs-lock");

static dev_t **chrdevs = NULL;
static spinlock_t *chrdevs_lock = SPINLOCK_NEW("chrdevs-lock");

static void kdev_blkdevs_lock(void)
{
    spin_lock(blkdevs_lock);
}

static void kdev_blkdevs_unlock(void)
{
    spin_unlock(blkdevs_lock);
}

static void kdev_chrdevs_lock(void)
{
    spin_lock(chrdevs_lock);
}

static void kdev_chrdevs_unlock(void)
{
    spin_unlock(chrdevs_lock);
}

int kdev_register(dev_t *dev, uint8_t major, uint8_t type)
{
    int err =0;
    if (!dev)
        return -EINVAL;
    switch(type)
    {
    case FS_BLKDEV:
        kdev_blkdevs_lock();
        if (blkdevs[major])
        {
            err = -EADDRINUSE;
            kdev_blkdevs_unlock();
            goto error;
        }
        if (type != FS_BLKDEV)
        {
            err = -EINVAL;
            kdev_blkdevs_unlock();
            goto error;
        }
        blkdevs[major] = dev;
        if (dev->dev_probe)
            dev->dev_probe();
        kdev_blkdevs_unlock();
        break;
    case FS_CHRDEV:
        kdev_chrdevs_lock();
        if (chrdevs[major])
        {
            err = -EADDRINUSE;
            kdev_chrdevs_unlock();
            goto error;
        }
        if (type != FS_CHRDEV)
        {
            err = -EINVAL;
            kdev_chrdevs_unlock();
            goto error;
        }
        chrdevs[major] = dev;
        if (dev->dev_probe)
            dev->dev_probe();
        kdev_chrdevs_unlock();
        break;
    default:
        err = -ENXIO;
        goto error;
    }
    printk("registered device '%s'\n", dev->dev_name);
    return 0;
error:
    printk("kdev_register(), called @ 0x%p, error=%d\n", return_address(0), err);
    return err;
}

dev_t *kdev_get(struct devid *dd)
{
    dev_t *dev = NULL;
    if (!dd)
        return NULL;
    switch(dd->dev_type)
    {
    case FS_BLKDEV:
        kdev_blkdevs_lock();
        dev = blkdevs[dd->dev_major];
        kdev_blkdevs_unlock();
        break;
    case FS_CHRDEV:
        kdev_chrdevs_lock();
        dev = chrdevs[dd->dev_major];
        kdev_chrdevs_unlock();
        break;
    default:
        dev = NULL;
    }
    return dev;
}

int kdev_open(struct devid *dd, int mode, ...)
{
    dev_t *dev = NULL;
    if (!dd)
        return -ENXIO;
    if (!(dev = kdev_get(dd)))
        return -ENXIO;
    if (!dev->devops.open)
        return -ENOSYS;
    return dev->devops.open(dd, mode);
}

int kdev_close(struct devid *dd)
{
    dev_t *dev = NULL;
    if (!dd)
        return -ENXIO;
    if (!(dev = kdev_get(dd)))
        return -ENXIO;
    if (!dev->devops.close)
        return -ENOSYS;
    return dev->devops.close(dd);
}

size_t kdev_read(struct devid *dd, off_t offset, void *buf, size_t sz)
{
    dev_t *dev = NULL;
    if (!dd)
        return -ENXIO;

    if (!(dev = kdev_get(dd)))
        return -ENXIO;
    if (!dev->devops.read)
        return -ENOSYS;
    return dev->devops.read(dd, offset, buf, sz);
}

size_t kdev_write(struct devid *dd, off_t offset, void *buf, size_t sz)
{
    dev_t *dev = NULL;

    if (!dd)
        return -ENXIO;

    if (!(dev = kdev_get(dd)))
        return -ENXIO;
    if (!dev->devops.write)
        return -ENOSYS;
    return dev->devops.write(dd, offset, buf, sz);
}

int kdev_ioctl(struct devid *dd, int request, void *argp)
{
    dev_t *dev = NULL;
    if (!dd)
        return -ENXIO;
    if (!(dev = kdev_get(dd)))
        return ENXIO;
    if (!dev->devops.ioctl)
        return -ENOSYS;
    return dev->devops.ioctl(dd, request, argp);
}

size_t kdev_lseek(struct devid *dd, off_t offset, int whence)
{
    struct dev *dev = NULL;
    if (!dd) return -ENXIO;
    if (!(dev = kdev_get(dd)))
        return -ENXIO;
    if (!dev->devops.lseek)
        return -ENXIO;
    return dev->devops.lseek(dd, offset, whence);
}

int kdev_load_devfs(dev_t ***devices)
{
    int err =0;
    
    for (int i = 0;  i < 256; ++i)
    {
        if (blkdevs[i])
            if ((err = blkdevs[i]->dev_mount()))
                goto error;

        if (chrdevs[i])
            if ((err = chrdevs[i]->dev_mount()))
                goto error;
    }

    devices[0] = blkdevs;
    devices[1] = chrdevs;
    devices[2] = NULL;

    return 0;
error:
    printk("kdev_load_devfs(), called @ 0x%p, error=%d\n", return_address(0), err);
    return err;
}

int dev_init(void)
{
    int err = 0;
    printk("initializing devs...\n");

    if (!(blkdevs = __cast_to_type(blkdevs) kcalloc(256, sizeof(dev_t))))
    {
        err = -ENOMEM;
        goto error;
    }
    printk("ongoing\n");
    if (!(chrdevs = __cast_to_type(blkdevs) kcalloc(256, sizeof(dev_t))))
    {
        err = -ENOMEM;
        goto error;
    }

    memset(blkdevs, 0, sizeof(dev_t) * 256);
    memset(chrdevs, 0, sizeof(dev_t) * 256);

    if ((err = modules_init()))
        return err;
    
    return 0;

error:
    if (blkdevs)
        kfree(blkdevs);
    if (chrdevs)
        kfree(chrdevs);
    printk("dev_init(): called @ 0x%p, error=%d\n", return_address(0), err);
    return err;
}