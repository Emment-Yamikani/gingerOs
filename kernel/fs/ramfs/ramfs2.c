#include <fs/fs.h>
#include <printk.h>
#include <fs/posix.h>
#include <fs/ramfs2.h>
#include <mm/kalloc.h>
#include <bits/errno.h>
#include <lime/string.h>

static iops_t ramfs2_iops;
static filesystem_t ramfs2;
static inode_t *iroot = NULL;
static super_block_t ramfs2_sb;
static inode_t *iramdisk = NULL;
static ramfs2_super_t *ramfs2_super = NULL;
static vmr_ops_t ramfs2_vmr_ops __unused;

int ramfs2_validate(ramfs2_super_t *super)
{
    uint32_t chksum = 0;
    const char *magic = "ginger_rd2";
    if (!super)
        return -EINVAL;
    if (compare_strings(super->header.magic, (char *)magic))
        return -EINVAL;
    for (int i = 0; i < __magic_len; ++i)
        chksum += (uint32_t)super->header.magic[i];
    chksum += super->header.ramfs2_size + super->header.super_size + super->header.checksum;
    for (uint32_t i = 0; i < super->header.nfile; ++i)
        chksum += super->nodes[i].size;
    return chksum;
}

static __unused ramfs2_node_t *ramfs2_convert_inode(inode_t *ip)
{
    if (ip == NULL) return NULL;
    return ip->i_priv;
}

int ramfs2_find(ramfs2_super_t *super, const char *fn, ramfs2_node_t **pnode)
{
    if (!super || !pnode)
        return -EINVAL;

    if (!fn || !*fn)
        return -ENOTNAM;

    if ((strlen(fn) >= __max_fname))
        return -ENAMETOOLONG;

    for (uint32_t indx = 0; indx < super->header.nfile; ++indx)
    {
        if (!compare_strings(super->nodes[indx].name, (char *)fn))
        {
            *pnode = &super->nodes[indx];
            return 0;
        }
    }

    return -ENOENT;
}

static int ramfs2_ifind(inode_t *dir, const char *name, inode_t **ref)
{
    int err = 0;
    inode_t *ip = NULL;
    ramfs2_node_t *node = NULL;

    if (!dir || !ref)
        return -EINVAL;

    if (dir != iroot)
        return -ENOTDIR;

    if ((err = ramfs2_find(ramfs2_super, name, &node)))
        return err;

    if ((err = ialloc(&ip)))
        return err;

    ip->ifs = &ramfs2;
    ip->i_priv = node;
    ip->i_gid = node->gid;
    ip->i_uid = node->uid;
    ip->i_mask = node->mode;
    ip->i_size = node->size;
    ip->i_type = (int[]) {
        [RAMFS2_INV] = FS_INV,
        [RAMFS2_REG] = FS_RGL,
        [RAMFS2_DIR] = FS_DIR,
    }[node->type];
    ip->i_ino = node - ramfs2_super->nodes;

    *ref = ip;
    return 0;
};

static int ramfs2_open(inode_t *ip __unused, int mode __unused, ...)
{
    return 0;
}

static size_t ramfs2_read(inode_t *ip, off_t off, void *buf, size_t sz)
{
    ramfs2_node_t *node = NULL;

    if (!ip || !buf)
        return -EINVAL;
    if ((node = ramfs2_convert_inode(ip)) == NULL)
        return -EINVAL;
    if (off >= node->size)
        return -1;
    sz = MIN((node->size - off), sz);
    off += node->offset + ramfs2_super->header.data_offset;
    return iread(iramdisk, off, buf, sz);
}

static size_t ramfs2_write(inode_t *ip __unused, off_t off __unused, void *buf __unused, size_t sz __unused)
{
    return -EROFS;
}

static int ramfs2_close(inode_t *ip __unused)
{
    return 0;
}

static int ramfs2_creat(inode_t *ip __unused, dentry_t *dentry __unused, int mode __unused)
{
    return -EROFS;
}

static int ramfs2_sync(inode_t *ip __unused)
{
    return -EROFS;
}

static int ramfs2_ioctl(inode_t *ip __unused, int req __unused, void *argp __unused)
{
    return -ENOTTY;
}

static int ramfs2_lseek(inode_t *ip __unused, off_t off __unused, int whence __unused)
{
    return -EINVAL;
}

static int ramfs2_readdir(inode_t *dir __unused, off_t offset __unused, struct dirent *dirent __unused)
{
    if (iroot != dir)
        return -EINVAL;
    return -ENOSYS;
}

static int ramfs2_chown(inode_t *ip __unused, uid_t uid __unused, gid_t gid __unused)
{
    return -EROFS;
}

int ramfs2_fault(vmr_t *vmr __unused, struct vm_fault *fault __unused)
{
    return -ENOSYS;
}

int ramfs2_mmap(file_t *file __unused, vmr_t *vmr __unused)
{
    return -ENOSYS;
}

static int ramfs2_read_super()
{
    int err = 0;
    size_t sbsz = 0;
    ramfs2_super_header_t hdr = {0};
    if ((iread(iramdisk, 0, &hdr, sizeof hdr)) != sizeof hdr)
        return -EFAULT;

    sbsz = sizeof hdr + (hdr.nfile * sizeof (ramfs2_node_t));

    if ((ramfs2_super = kcalloc(1, sbsz)) == NULL)
        return -ENOMEM;
    
    if ((iread(iramdisk, 0, ramfs2_super, sbsz)) != sbsz)
        return -EFAULT;
    
    if ((err = ramfs2_validate(ramfs2_super)))
        goto error;

    return 0;
error:
    if (ramfs2_super) kfree(ramfs2_super);
    printk("%s(): called @ 0x%p, error=%d\n", __func__, return_address(0), err);
    return err;
}

int ramfs2_init(void)
{
    int err = 0;
    printk("initilizing %s...\n", __func__);
    if ((err = vfs_register(&ramfs2)))
        goto error;
    return 0;
error:
    printk("%s(): called @ 0x%p, error=%d\n", __func__, return_address(0), err);
    return err;
}

int ramfs2_fsmount()
{
    int err = 0;
    if ((err = ialloc(&iroot)))
        goto error;

    iroot->ifs = &ramfs2;
    iroot->i_type = FS_DIR;
    iroot->i_mask = 0555;

    ramfs2_sb.s_iroot = iroot;
    ramfs2_sb.s_count = 1;

    if ((err = vfs_mount("/mnt", "ramfs", iroot)))
        goto error;

    if ((err = vfs_mount_root(iroot)))
        goto error;

    return 0;
error:
    if (iroot)
        irelease(iroot);
    iroot = NULL;
    klog(KLOG_FAIL, "Failed to mount ramfs2, error=%d\n", err);
    return err;
}

int ramfs2_load()
{
    int err = 0;
    uio_t uio = {.u_cwd = "/", .u_gid = 0, .u_uid = 0};
    if ((err = vfs_lookup("/dev/ramdisk", &uio, O_RDONLY | O_EXCL, &iramdisk, NULL)))
        goto error;
    if ((err = ramfs2_read_super()))
        goto error;
    return 0;
error:
    klog(KLOG_FAIL, "Failed to load ramfs2, error=%d\n", err);
    return err;
}

static iops_t ramfs2_iops = {
    .close = ramfs2_close,
    .creat = ramfs2_creat,
    .find = ramfs2_ifind,
    .bind = NULL,
    .chown = ramfs2_chown,
    .ioctl = ramfs2_ioctl,
    .lseek = ramfs2_lseek,
    .open = ramfs2_open,
    .read = ramfs2_read,
    .sync = ramfs2_sync,
    .write = ramfs2_write,
    .readdir = ramfs2_readdir,
};

static struct fops ramfs2_fops = (struct fops){
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
    .mmap = ramfs2_mmap,
};

static super_block_t ramfs2_sb = {
    .fops = &ramfs2_fops,
    .iops = &ramfs2_iops,
    .s_blocksz = 512,
    .s_magic = 0xc0deb00c,
    .s_maxfilesz = -1,
};

static filesystem_t ramfs2 = {
    .fname = "ramfs2",
    .flist_node = NULL,
    .fsuper = &ramfs2_sb,
    .load = ramfs2_load,
    .fsmount = ramfs2_fsmount,
};