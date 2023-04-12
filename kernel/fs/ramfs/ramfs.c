#include <arch/i386/cpu.h>
#include <bits/errno.h>
#include <fs/fs.h>
#include <fs/ramfs.h>
#include <lib/string.h>
#include <lime/string.h>
#include <mm/kalloc.h>
#include <printk.h>
#include <fs/posix.h>
#include <sys/_stat.h>
#include <sys/mman/mman.h>
#include <fs/ramfs2.h>

static iops_t ramfs_iops;
static inode_t *iroot = NULL;
static inode_t *iramdisk = NULL;
static ramfs_superblock_t ramfs_super = {0};
static filesystem_t ramfs;
static super_block_t ramfs_sb;
static vmr_ops_t ramfs_vmr_ops __unused;

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
    ip->ifs = &ramfs;
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

static int ramfs_find(inode_t *dir, const char *name, inode_t **ref)
{
    int err =0;
    inode_t *ip = NULL;

    if (!dir || !name || !ref)
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
            if (compare_strings(ramfs_super.inode[i].name, name))
                continue;

            if ((err = ramfs_ialloc(&ip)))
                goto error;

            ip->i_mask = 0555;
            ip->i_gid = ramfs_super.inode[i].gid;
            ip->i_uid = ramfs_super.inode[i].uid;
            ip->i_size = ramfs_super.inode[i].size;
            ip->i_priv = &ramfs_super.inode[i];
            ip->i_type = (int[]){
                [0] = FS_INV, [1] = FS_DIR, [2] = FS_RGL,
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
    return -ENOTTY;
}

static size_t ramfs_read(inode_t *ip, off_t off, void *buf, size_t sz)
{
    size_t size = 0;
    ramfs_inode_t *ramfs_inode = NULL;
    if (!ip)
        return -EINVAL;
    if (!(ramfs_inode = ramfs_convert(ip)))
        return -EINVAL;
    if ((int)off >= ramfs_inode->size)
        return -1;
    if ((off + sz) < off)
        return -EINVAL;
    if ((int)(off + sz) > ramfs_inode->size)
        sz = ramfs_inode->size - off;

    size = MIN(ramfs_inode->size - off, sz);
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

static int ramfs_readdir(inode_t *dir, off_t offset, struct dirent *dirent){
    if (iroot != dir)
        return -EINVAL;

    if (((int)offset < 0) || ((int)offset >= NELEM(ramfs_super.inode)))
        return -ERANGE;

    if (ramfs_super.inode[offset].type == INITRD_INV)
        return -EINVAL;

    safestrcpy(dirent->d_name, ramfs_super.inode[offset].name, strlen(ramfs_super.inode[offset].name) + 1);
    dirent->d_ino = offset;
    dirent->d_off = offset;
    dirent->d_reclen = sizeof *dirent;
    dirent->d_type = (int [])
    {
        [INITRD_INV] =  FS_INV,
        [INITRD_DIR] = _IFDIR,
        [INITRD_FILE]= _IFREG,
    }[ramfs_super.inode[offset].type];

    return 0;
}

static int ramfs_chown(inode_t *ip, uid_t uid, gid_t gid)
{
    ramfs_inode_t *inode = NULL;
    if (ip == NULL)
        return -EINVAL;
    
    ilock(ip);

    inode = ramfs_convert(ip);

    if (inode == NULL){
        iunlock(ip);
        return -EINVAL;
    }

    if (uid != -1)
    {
        inode->uid = uid;
        ip->i_uid = uid;
        iunlock(ip);
        return 0;
    }

    if (gid != -1)
    {
        inode->uid = gid;
        ip->i_gid = gid;
        iunlock(ip);
        return 0;
    }

    iunlock(ip);
    return 0;
}

int ramfs_fault(vmr_t *vmr, struct vm_fault *fault)
{
    (void)vmr;
    (void)fault;
    return -ENOSYS;
}

int ramfs_mmap(file_t *file, vmr_t *vmr)
{
    inode_t *ip = NULL;
    ramfs_inode_t *inode = NULL;
    if (file == NULL || vmr == NULL)
        return -EINVAL;

    if (file->f_inode == NULL)
        return -EINVAL;

    ip = file->f_inode;

    inode = ramfs_convert(ip);

    if (inode == NULL)
        return -EINVAL;

    if (inode->type != INITRD_FILE)
        return -EINVAL;

    vmr->file = ip;
    iincrement(ip);

    //vmr->vmrops = &ramfs_vmr_ops;

    return 0;
}

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
    if ((err = ramfs2_init()) == 0)
        return 0;
    if ((err = vfs_register(&ramfs)))
        goto error;
    return 0;
error:
    printk("ramfs_init(): called @ 0x%p, error=%d\n", return_address(0), err);
    return err;
}

int ramfs_fsmount()
{
    int err = 0;
    if ((err = ialloc(&iroot)))
        goto error;
    
    iroot->ifs = &ramfs;
    iroot->i_type = FS_DIR;
    iroot->i_mask = 0777;

    ramfs_sb.s_iroot = iroot;
    ramfs_sb.s_count = 1;

    if ((err = vfs_mount("/mnt", "ramfs", iroot)))
        goto error;

    if ((err = vfs_mount_root(iroot)))
        goto error;

    return 0;
error:
    if (iroot)
        irelease(iroot);
    iroot = NULL;
    klog(KLOG_FAIL, "Failed to mount ramfs, error=%d\n", err);
    return err;
}

int ramfs_load()
{
    int err = 0;
    uio_t uio = {.u_cwd = "/", .u_gid = 0, .u_uid = 0};
    mode_t mode = 0;
    if ((err = vfs_lookup("/dev/ramdisk", &uio, O_RDONLY | O_EXCL, mode, &iramdisk, NULL)))
        goto error;
    if ((err = ramfs_read_super()))
        goto error;
    return 0;
error:
    klog(KLOG_FAIL, "Failed to load ramfs, error=%d\n", err);
    return err;
}

static iops_t ramfs_iops ={
    .close = ramfs_close,
    .creat = ramfs_creat,
    .find = ramfs_find,
    .bind = NULL,
    .chown = ramfs_chown,
    .ioctl = ramfs_ioctl,
    .lseek = ramfs_lseek,
    .open = ramfs_open,
    .read = ramfs_read,
    .sync = ramfs_sync,
    .write = ramfs_write,
    .readdir = ramfs_readdir,
};

static struct fops ramfs_fops = (struct fops){
    .can_read = posix_file_can_read,
    .can_write = posix_file_can_write,
    .close = posix_file_close,
    .open = posix_file_open,
    .eof = posix_file_eof,
    .ioctl = posix_file_ioctl,
    .lseek = posix_file_lseek,
    .read = posix_file_read,
    .write = posix_file_write,
    .readdir = posix_file_readdir,
    .stat = posix_file_ffstat,
    .mmap = ramfs_mmap,
};

static super_block_t ramfs_sb = {
    .fops = &ramfs_fops,
    .iops = &ramfs_iops,
    .s_blocksz = 512,
    .s_magic = 0xc0deb00c,
    .s_maxfilesz = -1,
};

static filesystem_t ramfs = {
    .fname = "ramfs",
    .flist_node = NULL,
    .fsuper = &ramfs_sb,
    .load = ramfs_load,
    .fsmount = ramfs_fsmount,
};

static vmr_ops_t ramfs_vmr_ops = {
    .fault = ramfs_fault,
};