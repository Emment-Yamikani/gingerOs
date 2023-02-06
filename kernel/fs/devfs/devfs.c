#include <fs/fs.h>
#include <mm/kalloc.h>
#include <lib/string.h>
#include <printk.h>
#include <fs/devfs.h>
#include <bits/errno.h>
#include <dev/dev.h>
#include <lime/string.h>
#include <fs/fs.h>
#include <fs/posix.h>

struct devfs_dirent
{
    inode_t *inode;
};
struct devfsdir
{
    struct devfs_dirent *table;
    int ndirent;
};

__unused
static iops_t devfs_iops;
static filesystem_t devfs;


static dev_t **devices [3] = {NULL};

int kdev_load_devfs(dev_t ***);

int devfs_init(void)
{
    printk("initializing devfs...\n");
    return vfs_register(&devfs);
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

    if ((err = vfs_mountat(name, "/dev", "devfs", MS_BIND, NULL, idev, NULL)))
        goto error;

    printk("dev: \e[0;012m'%s'\e[0m mounted successfully...\n", name);
    return 0;
error:
    printk("\e[0;014mfailed to mount device: \e[0;012m%s\e[0m\e[0m\n", name);
    return err;
}

int devfs_find(inode_t *dir, const char *name, inode_t **ref)
{
    int err =0, is_blkdev = 1;
    inode_t *inode = NULL;

    if (!dir || !name || !ref)
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
                if (!compare_strings(array[i]->dev_name, name))
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

    if ((err = vfs_mountat(name, "/dev", "devfs", MS_BIND, NULL, inode, NULL)))
        goto error;

    *ref = inode;
    return 0;
error:
    if (inode)
        irelease(inode);
    printk("devfs_find(): called @ 0x%p, error=%d\n", return_address(0), err);
    return err;
}

int devfs_imount(inode_t *dir, const char *name __unused, inode_t *child){
    int bound = 0;
    struct devfsdir *devfs_dir = NULL;
    struct devfs_dirent *table = NULL;

    if (!dir || !child)
        return -EINVAL;

    ilock(dir);

    devfs_dir = dir->i_priv;

    if (devfs_dir == NULL)
    {
        devfs_dir = kmalloc(sizeof *devfs_dir);
        if (devfs_dir == NULL){
            iunlock(dir);
            return -ENOMEM;
        }
        
        table = kcalloc(1, sizeof (struct devfs_dirent));

        if (table == NULL)
        {
            iunlock(dir);
            kfree(devfs_dir);
            return -ENOMEM;
        }

        devfs_dir->table = table;
        devfs_dir->ndirent = 1;
        dir->i_priv = devfs_dir;
    }

    table = devfs_dir->table;

    assert(table, "how doesn't directory have no table?\n");

try_bind:
    //TODO: speed up binding by starting from suitable index
    for(int i = 0; i < devfs_dir->ndirent; ++i)
    {
        if (table[i].inode == NULL)
        {
            table[i].inode = child;
            ilock(child);
            child->ifs = &devfs;
            iunlock(child);
            bound = 1;
            break;
        }
    }

    if (bound){
        iunlock(dir);
        return 0;
    }

    table = krealloc(table, (devfs_dir->ndirent + 1) * sizeof (struct devfs_dirent));
    
    if (table == NULL){
        iunlock(dir);
        return -ENOMEM;
    }
    
    devfs_dir->table = table;
    table[devfs_dir->ndirent++].inode = NULL;

    goto try_bind;
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

static int devfs_readdir(inode_t *dir, off_t offset, struct dirent *dirent){
    inode_t *inode = 0;
    dentry_t *dentry = NULL;
    struct devfsdir *devfs_dir = NULL;
    struct devfs_dirent *table = NULL;

    if (dirent == NULL)
        return -EINVAL;

    if (!INODE_ISDIR(dir))
        return -ENOTDIR;
    
    ilock(dir);
    
    devfs_dir = dir->i_priv;

    if (devfs_dir == NULL){
        iunlock(dir);
        return -EINVAL;
    }

    if (((int)offset < 0) || ((int)offset >= devfs_dir->ndirent)){
        iunlock(dir);
        return -EINVAL;
    }

    table = devfs_dir->table;

    assert(table, "how doesn't directory have no table?\n");

    inode = table[offset].inode;

    if (inode == NULL){
        iunlock(dir);
        return -EINVAL;
    }

    ilock(inode);

    dentry = inode->i_dentry;

    if (dentry == NULL){
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

int devfs_load(){
    return 0;
}

int devfs_fsmount()
{
    int err = 0;
    inode_t *inode = NULL;
    printk("mounting devfs...\n");

    if ((err = ialloc(&inode)))
        goto error;

    inode->i_mask = 0755;
    inode->i_type = FS_DIR;
    inode->ifs = &devfs;

    if ((err = vfs_mountat("dev", "/", NULL, MS_BIND, NULL, inode, NULL)))
        goto error;

    if ((err = kdev_load_devfs(devices)))
        goto error;
    return 0;
error:
    return err;
}

static iops_t devfs_iops = 
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
    .readdir = devfs_readdir,
    .mount = devfs_imount,
};

static super_block_t devfs_sb = {
    .fops = &posix_fops,
    .iops = &devfs_iops,
    .s_blocksz = 512,
    .s_magic = 0xDEF1CE,
    .s_maxfilesz = -1,
};

static filesystem_t devfs = {
    .fname = "devfs",
    .flist_node = NULL,
    .fsuper = &devfs_sb,
    .load = devfs_load,
    .fsmount = devfs_fsmount,
};