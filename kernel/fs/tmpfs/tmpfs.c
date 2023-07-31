#include <fs/fs.h>
#include <mm/kalloc.h>
#include <lib/string.h>
#include <printk.h>
#include <fs/tmpfs.h>
#include <bits/errno.h>
#include <dev/dev.h>
#include <lime/string.h>
#include <fs/fs.h>
#include <fs/posix.h>

struct tmpfs_dirent
{
    char *name;
    inode_t *inode;
};

struct tmpfsdir
{
    struct tmpfs_dirent *table;
    int ndirent;
};

static iops_t tmpfs_iops;
static filesystem_t tmpfs;

int tmpfs_init(void)
{
    printk("initializing tmpfs...\n");
    return vfs_register(&tmpfs);
}

static int tmpfs_open(inode_t *ip __unused, int mode __unused, ...)
{
    return 0;
}

static int tmpfs_close(inode_t *ip __unused)
{
    return -EINVAL;
}

static int tmpfs_find(inode_t *dir, const char *name, inode_t **ref)
{
    int err = 0;
    inode_t *inode = 0;
    struct tmpfsdir *tmpfs_dir = NULL;
    struct tmpfs_dirent *table = NULL;

    if (!dir || !name || !ref)
        return -EINVAL;

    ilock(dir);

    if (!INODE_ISDIR(dir))
    {
        iunlock(dir);
        return -ENOTDIR;
    }

    tmpfs_dir = dir->i_priv;

    if (tmpfs_dir == NULL)
    {
        iunlock(dir);
        return -ENOENT;
    }

    table = tmpfs_dir->table;

    assert(table, "how doesn't directory have no table?\n");

    for (int i = 0; i < tmpfs_dir->ndirent; ++i)
    {
        inode = table[i].inode;
        if (table[i].name == NULL)
            continue;

        if (inode == NULL)
        {
            iunlock(inode);
            iunlock(dir);
            return -EINVAL;
        }

        ilock(inode);
        if (!compare_strings(table[i].name, name))
        {
            *ref = inode;
            iunlock(inode);
            iunlock(dir);
            return 0;
        }
        iunlock(inode);
    }

    iunlock(dir);
    return -ENOENT;
    goto error;
error:
    printk("tmpfs_find(): called @ 0x%p, error=%d\n", return_address(0), err);
    return err;
}

static int tmpfs_mount(inode_t *dir, const char *name, inode_t *child)
{
    int bound = 0;
    __unused dentry_t *dentry = NULL;
    struct tmpfsdir *tmpfs_dir = NULL;
    struct tmpfs_dirent *table = NULL;

    if (!dir || !child)
        return -EINVAL;

    ilock(dir);

    tmpfs_dir = dir->i_priv;

    if (tmpfs_dir == NULL)
    {
        tmpfs_dir = kmalloc(sizeof *tmpfs_dir);
        if (tmpfs_dir == NULL)
        {
            iunlock(dir);
            return -ENOMEM;
        }

        table = kcalloc(1, sizeof(struct tmpfs_dirent));

        if (table == NULL)
        {
            iunlock(dir);
            kfree(tmpfs_dir);
            return -ENOMEM;
        }

        tmpfs_dir->table = table;
        tmpfs_dir->ndirent = 1;
        dir->i_priv = tmpfs_dir;
    }

    table = tmpfs_dir->table;

    assert(table, "how doesn't directory have no table?\n");

try_bind:
    // TODO: speed up binding by starting from suitable index
    for (int i = 0; i < tmpfs_dir->ndirent; ++i)
    {
        if (table[i].inode == NULL)
        {
            if ((table[i].name = strdup(name)) == NULL)
            {
                iunlock(dir);
                return -ENOMEM;
            }

            table[i].inode = child;
            ilock(child);
            if ((child->ifs == NULL))
                child->ifs = &tmpfs;
            iunlock(child);
            bound = 1;
            break;
        }
    }

    if (bound){
        iunlock(dir);
        return 0;
    }

    table = krealloc(table, (tmpfs_dir->ndirent + 1) * sizeof(struct tmpfs_dirent));

    if (table == NULL)
    {
        iunlock(dir);
        return -ENOMEM;
    }

    tmpfs_dir->table = table;
    table[tmpfs_dir->ndirent].inode = NULL;
    table[tmpfs_dir->ndirent++].name = NULL;

    goto try_bind;
}

static int tmpfs_create(inode_t *dir, dentry_t *dentry, int mode)
{
    int err = 0;
    inode_t *inode = NULL;
    if ((err = ialloc(&inode)))
        goto error;

    inode->i_mask = mode;
    inode->ifs = dir->ifs;
    inode->i_type = FS_RGL;
    dentry->d_inode = inode;
    inode->i_dentry = dentry;
    inode->i_gid = dir->i_gid;
    inode->i_uid = dir->i_uid;

    if ((err = tmpfs_mount(dir, dentry->d_name, inode)))
        goto error;

    return 0;
error:
    if (inode)
        irelease(inode);
    return err;
}

static int tmpfs_ioctl(inode_t *ip __unused, int req __unused, void *argp __unused)
{
    return -ENOTTY;
}

static int tmpfs_lseek(inode_t *ip __unused, off_t off __unused, int whence __unused)
{
    return -EINVAL;
}

static size_t tmpfs_read(inode_t *ip, off_t off, void *buf, size_t sz)
{
    char *data __unused = NULL;
    if (!ip || !buf)
        return -EINVAL;

    if (off >= ip->i_size || (!ip->i_priv))
        return -1;

    size_t size = MIN((ip->i_size - off), sz);
    data = ip->i_priv;
    memcpy(buf, data + off, size);
    return size;
}

static int tmpfs_sync(inode_t *ip __unused)
{
    return -EINVAL;
}

static size_t tmpfs_write(inode_t *ip, off_t off, void *buf, size_t sz)
{
    size_t size = 0;
    void *data = NULL;

    if (!ip || !buf)
        return -EINVAL;
    
    if (INODE_ISDIR(ip))
        return -EISDIR;

    if ((off + sz) > ip->i_size) {
        if (!ip->i_priv) {
            data = kcalloc(1, off + sz);
            if (!data)
                return -EAGAIN;
        } else {
            data = krealloc(ip->i_priv, off + sz);
            if (!data)
                return -EAGAIN;
        }
        ip->i_size = off + sz;
        ip->i_priv = data;
    }

    size = MIN((ip->i_size - off), sz);
    memcpy(data + off, buf, size);

    return size;
}

static int tmpfs_readdir(inode_t *dir, off_t offset, struct dirent *dirent)
{
    inode_t *inode = 0;
    dentry_t *dentry = NULL;
    struct tmpfsdir *tmpfs_dir = NULL;
    struct tmpfs_dirent *table = NULL;

    if (dirent == NULL)
        return -EINVAL;

    if (!INODE_ISDIR(dir))
        return -ENOTDIR;

    ilock(dir);

    tmpfs_dir = dir->i_priv;

    if (tmpfs_dir == NULL)
    {
        iunlock(dir);
        return -EINVAL;
    }

    if (((int)offset < 0) || ((int)offset >= tmpfs_dir->ndirent))
    {
        iunlock(dir);
        return -EINVAL;
    }

    table = tmpfs_dir->table;

    assert(table, "how doesn't directory have no table?\n");

    inode = table[offset].inode;

    if (inode == NULL)
    {
        iunlock(dir);
        return -EINVAL;
    }

    ilock(inode);

    dentry = inode->i_dentry;

    if (dentry == NULL)
    {
        iunlock(inode);
        iunlock(dir);
        return -EINVAL;
    }

    dlock(dentry);

    safestrcpy(dirent->d_name, dentry->d_name, strlen(dentry->d_name) + 1);
    dirent->d_ino = inode->i_ino;
    dirent->d_off = offset;
    dirent->d_reclen = sizeof *dirent;
    dirent->d_type = (int[]){
        [FS_INV] = 0,
        [FS_RGL] = _IFREG,
        [FS_DIR] = _IFDIR,
        [FS_BLKDEV] = _IFBLK,
        [FS_CHRDEV] = _IFCHR,
        [FS_PIPE] = _IFIFO,
    }[inode->i_type];

    dunlock(dentry);
    iunlock(inode);
    iunlock(dir);
    return 0;
}

static int tmpfs_mmap(file_t *file __unused, vmr_t *region __unused) {
    int err = 0;
    off_t offset __unused = 0;
    ssize_t size __unused = 0;
    inode_t *inode = NULL;

    if (!file || !region)
        return -EINVAL;

    inode = file->f_inode;

    if (!inode)
        return -EINVAL;
    
    size = (region->file_pos + region->filesz);

    

    return 0;
    return err;
}

static int tmpfs_load()
{
    return 0;
}

static int tmpfs_fsmount()
{
    int err = 0;
    inode_t *inode = NULL;
    printk("mounting tmpfs...\n");

    if ((err = ialloc(&inode)))
        goto error;

    inode->i_mask = 0755;
    inode->i_type = FS_DIR;
    inode->ifs = &tmpfs;

    if ((err = vfs_mount("/", "tmp", inode)))
        goto error;
    return 0;
error:
    return err;
}

static iops_t tmpfs_iops = {
    .open = tmpfs_open,
    .close = tmpfs_close,
    .creat = tmpfs_create,
    .find = tmpfs_find,
    .ioctl = tmpfs_ioctl,
    .lseek = tmpfs_lseek,
    .read = tmpfs_read,
    .sync = tmpfs_sync,
    .write = tmpfs_write,
    .readdir = tmpfs_readdir,
    .mount = tmpfs_mount,
};

__unused static struct fops tmpfs_fops = (struct fops){
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
    .mmap = tmpfs_mmap,
};

static super_block_t tmpfs_sb = {
    .fops = &posix_fops,
    .iops = &tmpfs_iops,
    .s_blocksz = 512,
    .s_magic = 0xDEF1CE,
    .s_maxfilesz = -1,
};

static filesystem_t tmpfs = {
    .fname = "tmpfs",
    .flist_node = NULL,
    .fsuper = &tmpfs_sb,
    .load = tmpfs_load,
    .fsmount = tmpfs_fsmount,
};