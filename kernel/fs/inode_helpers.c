#include <bits/errno.h>
#include <dev/dev.h>
#include <fs/fs.h>
#include <lib/string.h>
#include <lime/string.h>
#include <mm/kalloc.h>
#include <printk.h>

#define CHK_IPTR(ip)                \
    {                               \
        if (!ip->ifs)               \
            return -ENOSYS;         \
        if (!ip->ifs->fsuper)       \
            return -ENOSYS;         \
        if (!ip->ifs->fsuper->iops) \
            return -ENOSYS;         \
    }

int iopen(inode_t *ip, int mode, ...)
{
    if (!ip)
        return -EINVAL;
    if (ISDEV(ip))
        return kdev_open(_INODE_DEV(ip), mode);
    
    CHK_IPTR(ip);

    if (!ip->ifs->fsuper->iops->open)
        return -ENOSYS;

    return ip->ifs->fsuper->iops->open(ip, mode);
}

int iclose(inode_t *ip)
{
    if (!ip)
        return -EINVAL;
    if (ISDEV(ip))
        return kdev_close(_INODE_DEV(ip));
    
    CHK_IPTR(ip);

    if (!ip->ifs->fsuper->iops->close)
        return -ENOSYS;

    return ip->ifs->fsuper->iops->close(ip);
}

size_t iread(inode_t *ip, off_t pos, void *buf, size_t sz)
{
    if (!ip)
        return -EINVAL;
    if (ISDEV(ip))
        return kdev_read(_INODE_DEV(ip), pos, buf, sz);
    
    CHK_IPTR(ip);

    if (!ip->ifs->fsuper->iops->read)
        return -ENOSYS;

    return ip->ifs->fsuper->iops->read(ip, pos, buf, sz);
}

size_t iwrite(inode_t *ip, off_t pos, void *buf, size_t sz)
{
    if (!ip)
        return -EINVAL;
    if (ISDEV(ip))
        return kdev_write(_INODE_DEV(ip), pos, buf, sz);
    
    CHK_IPTR(ip);

    if (!ip->ifs->fsuper->iops->write)
        return -ENOSYS;

    return ip->ifs->fsuper->iops->write(ip, pos, buf, sz);
}

int iioctl(inode_t *ip, int req, void *argp)
{
    if (!ip)
        return -EINVAL;
    if (ISDEV(ip))
        return kdev_ioctl(_INODE_DEV(ip), req, argp);
    
    CHK_IPTR(ip);

    if (!ip->ifs->fsuper->iops->ioctl)
        return -ENOSYS;

    return ip->ifs->fsuper->iops->ioctl(ip, req, argp);
}

int ifind(inode_t *dir, dentry_t *dentry, inode_t **ref)
{
    if (!dir || !dentry || !ref)
        return -EINVAL;
    if (ISDEV(dir) || dir->i_type != FS_DIR)
        return -ENOTDIR;
    
    CHK_IPTR(dir);
    printk("ifind\n");

    if (!dir->ifs->fsuper->iops->find)
        return -ENOSYS;

    return dir->ifs->fsuper->iops->find(dir, dentry, ref);
}

/* check for file permission */
int iperm(inode_t *ip, uio_t *uio, int mode)
{
    //printk("%s(\e[0;15mip=%p, uio=%p, mode=%d)\e[0m\n", __func__, ip, uio, mode);
    if (!ip || !uio)
        return -EINVAL;

    if (uio->u_uid == 0) /* root */
    {
        //printk("%s(): \e[0;12maccess granted, by virtue of being root\e[0m\n", __func__);
        return 0;
    }

    if (((mode & O_ACCMODE) == O_RDONLY) || (mode & O_ACCMODE) != O_WRONLY)
    {
        if (ip->i_uid == uio->u_uid)
        {
            if (ip->i_mask & S_IRUSR)
                goto write_perms;
        }
        else if (ip->i_gid == uio->u_gid)
        {
            if (ip->i_mask & S_IRGRP)
                goto write_perms;
        }
        else
        {
            if (ip->i_mask & S_IROTH)
                goto write_perms;
        }

        return -EACCES;
    }

write_perms:
    if (((mode & O_ACCMODE) == O_WRONLY) || (mode & O_ACCMODE) == O_RDWR)
    {
        if (ip->i_uid == uio->u_uid)
        {
            if (ip->i_mask & S_IWRITE)
                goto exec_perms;
        }
        else if (ip->i_gid == uio->u_gid)
        {
            if (ip->i_mask & S_IWGRP)
                goto exec_perms;
        }
        else
        {
            if (ip->i_mask & S_IWOTH)
                goto exec_perms;
        }

        return -EACCES;
    }

exec_perms:
    if ((mode & O_EXCL))
    {
        if (ip->i_uid == uio->u_uid)
        {
            if (ip->i_mask & S_IEXEC)
                goto done;
        }
        else if (ip->i_gid == uio->u_gid)
        {
            if (ip->i_mask & S_IXGRP)
                goto done;
        }
        else
        {
            if (ip->i_mask & S_IXOTH)
                goto done;
        }
        return -EACCES;
    }
done:
    //printk("%s(): \e[0;12maccess granted\e[0m\n", __func__);
    return 0;
}

int ilseek(inode_t *ip, off_t off, int whence)
{
    if (!ip)
        return -EINVAL;
    if (ISDEV(ip))
        return kdev_lseek(_INODE_DEV(ip), off, whence);
    
    CHK_IPTR(ip);

    if (!ip->ifs->fsuper->iops->lseek)
        return -ENOSYS;

    return ip->ifs->fsuper->iops->lseek(ip, off, whence);
}

int istat(inode_t *ip, struct stat *buf)
{
    if (!ip)
        return -EINVAL;

    buf->st_dev = 0; /* TODO */
    buf->st_ino = 0; /* TODO */

    ilock(ip);
    buf->st_mode = (int[]){
        [FS_RGL] = _IFREG,
        [FS_DIR] = _IFDIR,
        [FS_CHRDEV] = _IFCHR,
        [FS_BLKDEV] = _IFBLK,
        [FS_PIPE] = _IFIFO,
        //[FS_SYMLINK] = _IFLNK,
        //[FS_FIFO]    = _IFIFO,
        //[FS_SOCKET]  = _IFSOCK,
        //[FS_SPECIAL] = 0    /* FIXME */
    }[ip->i_type];

    buf->st_mode |= ip->i_mask;
    buf->st_nlink = 0; // ip->i_nlink;
    buf->st_uid = ip->i_uid;
    buf->st_gid = ip->i_gid;
    buf->st_rdev = ip->i_rdev;
    buf->st_size = ip->i_size;
    iunlock(ip);
    return 0;
}