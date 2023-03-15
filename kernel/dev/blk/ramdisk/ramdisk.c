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
#include <fs/posix.h>

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

#define ramdisk_lock() spin_lock(ramdisklock)

#define ramdisk_unlock() spin_unlock(ramdisklock)

int ramdisk_probe(void)
{
    return 0;
}

int ramdisk_mount()
{
    dev_attr_t attr ={
        .devid = *_DEVID(FS_BLKDEV, _DEV_T(DEV_RAMDISK, 0)),
        .size = ramdisk_size,
        .mask = 0444,
    };
    return devfs_mount("ramdisk", attr);
}

size_t ramdisk_read(struct devid *dd __unused, off_t off, void *buf, size_t sz)
{
    int locked = 0;
    size_t size = MIN(sz, ramdisk_size - off);
    if (!spin_holding(ramdisklock)){
        ramdisk_lock();
        locked = 1;
    }

    memcpy(buf, ramdisk_addr + off, size);

    if (spin_holding(ramdisklock) && locked)
    {
        ramdisk_unlock();
        locked = 0;
    }
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
    },

    .fops =
    {
        .close = posix_file_close,
        .ioctl = posix_file_ioctl,
        .lseek = posix_file_lseek,
        .open = posix_file_open,
        .perm = NULL,
        .read = posix_file_read,
        .sync = NULL,
        .stat = posix_file_ffstat,
        .write = posix_file_write,
        
        .can_read = (size_t(*)(struct file *, size_t))__always,
        .can_write = (size_t(*)(struct file *, size_t))__always,
        .eof = (size_t(*)(struct file *))__never
    }
};

MODULE_INIT(ramdisk, ramdisk_init, NULL)