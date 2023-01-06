#include <printk.h>
#include <dev/dev.h>
#include <lib/stdint.h>
#include <locks/spinlock.h>
#include <sys/system.h>
#include <arch/boot/boot.h>
#include <lib/errno.h>
#include <lib/string.h>
#include <fs/fs.h>
#include <arch/i386/cpu.h>
#include <lime/module.h>
#include <fs/devfs.h>

struct dev ramdiskdev;
spinlock_t *ramdisklock = SPINLOCK_NEW("ramdisklock");

char *ramdisk_addr =0;
size_t ramdisk_size = 0;

int ramdisk_init(void)
{
    int err = 0;
    if (!bootinfo.mods[0].addr)
        return -ENXIO;
    ramdisk_size = bootinfo.mods[0].size;
    ramdisk_addr = (char *)bootinfo.mods[0].addr;
    if (!ramdisk_addr)
        return -ENXIO;
    if ((err = kdev_register(&ramdiskdev, DEV_RAMDISK, FS_BLKDEV)))
        goto error;
    return 0;
error:
    return err;
}

static void ramdisk_lock(void)
{
    spin_lock(ramdisklock);
}

static void ramdisk_unlock(void)
{
    spin_unlock(ramdisklock);
}

int ramdisk_probe(void)
{
    return 0;
}

int ramdisk_mount()
{
    return devfs_mount("ramdisk", 0444, *_DEVID(FS_BLKDEV, _DEV_T(DEV_RAMDISK, 0)));
}

size_t ramdisk_read(struct devid *dd __unused, off_t off, void *buf, size_t sz)
{
    size_t size = MIN(sz, ramdisk_size - off);
    ramdisk_lock();
    memcpy(buf, ramdisk_addr + off, size);
    ramdisk_unlock();
    return size;
}

size_t ramdisk_write(struct devid *dd __unused, off_t off __unused, void *buf __unused, size_t sz __unused)
{
    size_t size = MIN(sz, ramdisk_size - off);
    ramdisk_lock();
    memcpy(ramdisk_addr + off, buf, size);
    ramdisk_unlock();
    return size;
}

int ramdisk_open(struct devid *dd __unused, int mode __unused, ...)
{
    return -EINVAL;
}

int ramdisk_close(struct devid *dd __unused)
{
    return -EINVAL;
}

int ramdisk_ioctl(struct devid *dd __unused, int request __unused, void *argp __unused)
{
    return -EINVAL;
}

struct dev ramdiskdev = 
{
    .dev_name = "ramdisk",
    .dev_probe = ramdisk_probe,
    .dev_mount = ramdisk_mount,
    .devid = _DEV_T(DEV_RAMDISK, 0),
    .devops = 
    {
        .open = ramdisk_open,
        .read = ramdisk_read,
        .write = ramdisk_write,
        .ioctl = ramdisk_ioctl,
        .close = ramdisk_close
    }
};

MODULE_INIT(ramdisk, ramdisk_init, NULL)