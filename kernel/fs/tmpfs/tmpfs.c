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

static int tmpfs_creat(inode_t *ip __unused, dentry_t *dentry __unused, int mode __unused)
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

static int tmpfs_ioctl(inode_t *ip __unused, int req __unused, void *argp __unused)
{
    return -EINVAL;
}

static int tmpfs_lseek(inode_t *ip __unused, off_t off __unused, int whence __unused)
{
    return -EINVAL;
}

static size_t tmpfs_read(inode_t *ip __unused, off_t off __unused, void *buf __unused, size_t sz __unused)
{
    return -EINVAL;
}

static int tmpfs_sync(inode_t *ip __unused)
{
    return -EINVAL;
}

static size_t tmpfs_write(inode_t *ip __unused, off_t off __unused, void *buf __unused, size_t sz __unused)
{
    return -EINVAL;
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
    .creat = tmpfs_creat,
    .find = tmpfs_find,
    .ioctl = tmpfs_ioctl,
    .lseek = tmpfs_lseek,
    .read = tmpfs_read,
    .sync = tmpfs_sync,
    .write = tmpfs_write,
    .readdir = tmpfs_readdir,
    .mount = tmpfs_mount,
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