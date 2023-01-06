#include <arch/i386/cpu.h>
#include <bits/errno.h>
#include <fs/fs.h>
#include <fs/ramfs.h>
#include <lib/string.h>
#include <lime/string.h>
#include <mm/kalloc.h>
#include <printk.h>

static iops_t ramfs_iops;
static inode_t *iroot = NULL;
static inode_t *iramdisk = NULL;
static ramfs_superblock_t ramfs_super = {0};
static filesystem_t ramfs;
static super_block_t ramfs_sb;

static int ramfs_ialloc(inode_t **ref)
{
    int err = 0;
    inode_t *ip = NULL;
    if (!ref)
        return -EINVAL;
    if ((err = ialloc(&ip)))
        goto error;
    ip->i_mask = 0555;
    ip->i_type = FS_RGL;
    ip->iops = ramfs_iops;
    *ref = ip;
    return 0;
error:
    printk("ramfs_ialloc(): called @ 0x%p, error=%d\n", return_address(0), err);
    return err;
}

static ramfs_inode_t *ramfs_convert(inode_t *ip)
{
    if (!ip)
        return NULL;
    return (ramfs_inode_t *)ip->i_priv;
}

static int ramfs_open(inode_t *ip __unused, int mode __unused, ...)
{
    return 0;
}

static int ramfs_close(inode_t *ip __unused)
{
    return -EINVAL;
}

static int ramfs_creat(inode_t *ip __unused, dentry_t *dentry __unused, int mode __unused)
{
    return -EROFS;
}

static int ramfs_find(inode_t *dir, dentry_t *dentry, inode_t **ref)
{
    int err =0;
    inode_t *ip = NULL;

    if (!dir || !dentry || !ref)
        return -EINVAL;
    
    if (dir != iroot)
    {
        err = -ENOENT;
        goto error;
    }

    for (int i =0; i < NELEM(ramfs_super.inode); ++i)
    {
        switch (ramfs_super.inode[i].type)
        {
        case INITRD_INV:
            continue;
        case INITRD_FILE:
            if (compare_strings(ramfs_super.inode[i].name, dentry->d_name))
                continue;

            if ((err = ramfs_ialloc(&ip)))
                goto error;
            
            ip->i_mask = 0555;
            ip->i_gid = ramfs_super.inode[i].gid;
            ip->i_uid = ramfs_super.inode[i].uid;
            ip->i_size = ramfs_super.inode[i].size;
            ip->i_priv = &ramfs_super.inode[i];
            ip->i_type = (int[])
            {
                [0] = FS_INV, [1] = FS_DIR, [2] = FS_RGL
            }[ramfs_super.inode[i].type];
            goto found;
        default:
            err = -EBADF;
            goto error;
        }
    }

    err = -ENOENT;
    goto error;

found:
    dentry->d_inode = ip;
    *ref = ip;
    return 0;
error:
    if (ip)
        irelease(ip);
    printk("ramfs_find(): called @ 0x%p, error=%d\n", return_address(0), err);
    return err;
}

static int ramfs_sync(inode_t *ip __unused)
{
    return -EINVAL;
}

static int ramfs_ioctl(inode_t *ip __unused, int req __unused, void *argp __unused)
{
    return -EINVAL;
}

static size_t ramfs_read(inode_t *ip, off_t off, void *buf, size_t sz)
{
    size_t size = 0;
    ramfs_inode_t *ramfs_inode = NULL;
    if (!ip)
        return -EINVAL;
    if (off > ip->i_size)
        return -EOVERFLOW;
    if (!(ramfs_inode = ramfs_convert(ip)))
        return -EINVAL;
    size = MIN(ip->i_size - off, sz);
    return iread(iramdisk, ramfs_inode->offset + off, buf, size);
}

static size_t ramfs_write(inode_t *ip __unused, off_t off __unused, void *buf __unused, size_t sz __unused)
{
    return -EROFS;
}

static int ramfs_lseek(inode_t *ip __unused, off_t off __unused, int whence __unused)
{
    return -EINVAL;
}

static iops_t ramfs_iops ={
    .close = ramfs_close,
    .creat = ramfs_creat,
    .find = ramfs_find,
    .ioctl = ramfs_ioctl,
    .lseek = ramfs_lseek,
    .open = ramfs_open,
    .read = ramfs_read,
    .sync = ramfs_sync,
    .write = ramfs_write
};

static int ramfs_read_super()
{
    int err=0;
    size_t size = 0;

    size = iread(iramdisk, 0, &ramfs_super, sizeof ramfs_super);
    if (size != sizeof ramfs_super)
    {
        err = -EFAULT;
        goto error;
    }

    if (compare_strings((char *)ramfs_super.magic, "gingerrd1"))
    {
        err = -ENOTSUP;
        goto error;
    }

    return 0;
error:
    printk("ramfs_read_super(): called @ 0x%p, error=%d\n", return_address(0), err);
    return err;
}

int ramfs_init(void)
{
    int err =0;
    uio_t uio = {.u_cwd = "/", .u_gid =0, .u_uid =0};
    if ((err = vfs_lookup("/dev/ramdisk", &uio, O_RDONLY | O_EXCL, &iramdisk, NULL)))
        goto error;
    
    if ((err = ramfs_ialloc(&iroot)))
        goto error;
    
    iroot->i_type = FS_DIR;

    if ((err = vfs_mount("/mnt", "ramfs", iroot)))
        goto error;
    
    if ((err = ramfs_read_super()))
        goto error;

    if ((err = vfs_mount_root(iroot)))
        goto error;

    return 0;
error:
    printk("ramfs_init(): called @ 0x%p, error=%d\n", return_address(0), err);
    return err;
}

static super_block_t ramfs_sb = {

};

static filesystem_t ramfs = {
    .fname = "ramfs",
    .flist_node = NULL,
    .fsuper = &ramfs_sb,
};