#include <fs/fs.h>
#include <mm/kalloc.h>
#include <lib/string.h>
#include <printk.h>
#include <fs/devfs.h>
#include <bits/errno.h>
#include <dev/dev.h>
#include <lime/string.h>
#include <fs/fs.h>

__unused
static iops_t iops;

static dev_t **devices [3] = {NULL};

int kdev_load_devfs(dev_t ***);

int devfs_init(void)
{
    int err =0;
    inode_t *inode = NULL;
    printk("initializing devfs...\n");

    if ((err = ialloc(&inode)))
        goto error;

    inode->i_mask = 0755;
    inode->i_type = FS_DIR;

    if ((err = vfs_mount("/", "dev", inode)))
        goto error;

    if ((err = kdev_load_devfs(devices)))
        goto error;
    return 0;
error:
    return err;
}

int devfs_open(inode_t *ip __unused, int mode __unused, ...)
{
    return 0;
}

int devfs_close(inode_t *ip __unused)
{
    return -EINVAL;
}

int devfs_creat(inode_t *ip __unused, dentry_t *dentry __unused, int mode __unused)
{
    return -EINVAL;
}

int devfs_mount(const char *name, dev_attr_t attr)
{
    int err=0;
    dev_t *dev = NULL;
    inode_t *idev = NULL;
    
    if (!name)
        return -EINVAL;

    if (!(dev = kdev_get(&attr.devid)))
    {
        err = -ENXIO;
        goto error;
    }

    if ((err = ialloc(&idev)))
        return err;

    idev->i_mask = attr.mask;
    idev->i_type = attr.devid.dev_type;
    idev->i_rdev = _DEV_T(attr.devid.dev_major, attr.devid.dev_minor);
    idev->i_size = attr.size;
    dev->dev_inode = idev;

    if ((err = vfs_mount("/dev", name, idev)))
        goto error;

    printk("dev: \e[0;012m'%s'\e[0m mounted successfully...\n", name);
    return 0;
error:
    printk("\e[0;014mfailed to mount device: \e[0;012m%s\e[0m\e[0m\n", name);
    return err;
}

int devfs_find(inode_t *dir, dentry_t *dentry, inode_t **ref)
{
    int err =0, is_blkdev = 1;
    inode_t *inode = NULL;

    if (!dir || !dentry || !ref)
    {
        err = -EINVAL;
        goto error;
    }

    foreach(array, devices)
    {
        for (int i =0; i < 256; ++i)
        {
            if (array[i])
            {
                if (!compare_strings(array[i]->dev_name, dentry->d_name))
                    goto found;
            }
        }
        is_blkdev =0;
    }

    err = -ENOENT;
    goto error;
found:
    if ((err = ialloc(&inode)))
        goto error;

    inode->i_mask = 0755;
    if (is_blkdev)
        inode->i_type = FS_BLKDEV;
    else
        inode->i_type = FS_CHRDEV;

    if ((err = vfs_mount("/dev/", dentry->d_name, inode)))
        goto error;

    *ref = inode;
    return 0;
error:
    if (inode)
        irelease(inode);
    printk("devfs_find(): called @ 0x%p, error=%d\n", return_address(0), err);
    return err;
}

int devfs_ioctl(inode_t *ip __unused, int req __unused, void *argp __unused)
{
    return -EINVAL;
}

int devfs_lseek(inode_t *ip __unused, off_t off __unused, int whence __unused)
{
    return -EINVAL;
}

size_t devfs_read(inode_t *ip __unused, off_t off __unused, void *buf __unused, size_t sz __unused)
{
    return -EINVAL;
}

int devfs_sync(inode_t *ip __unused)
{
    return -EINVAL;
}

size_t devfs_write(inode_t *ip __unused, off_t off __unused, void *buf __unused, size_t sz __unused)
{
    return -EINVAL;
}

static iops_t iops = 
{
    .open = devfs_open,
    .close = devfs_close,
    .creat = devfs_creat,
    .find = devfs_find,
    .ioctl = devfs_ioctl,
    .lseek = devfs_lseek,
    .read = devfs_read,
    .sync = devfs_sync,
    .write = devfs_write,
};