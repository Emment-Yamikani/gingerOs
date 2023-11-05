#include <bits/errno.h>
#include <dev/dev.h>
#include <fs/fs.h>
#include <lib/string.h>
#include <lime/string.h>
#include <mm/kalloc.h>
#include <arch/i386/paging.h>
#include <printk.h>

#define CHK_IPTR(ip)            \
{                               \
    if (!ip->ifs)               \
        return -ENOSYS;         \
    if (!ip->ifs->fsuper)       \
        return -ENOSYS;         \
    if (!ip->ifs->fsuper->iops) \
        return -ENOSYS;         \
}

int iopen(inode_t *ip, int oflags, ...)
{
    if (!ip)
        return -EINVAL;
    if (ISDEV(ip))
        return kdev_open(_INODE_DEV(ip), oflags);
    
    CHK_IPTR(ip);

    if (!ip->ifs->fsuper->iops->open)
        return -ENOSYS;

    return ip->ifs->fsuper->iops->open(ip, oflags);
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
    size_t size= 0;
    int holding = 0;
    ssize_t retval = 0;
    page_t *page = NULL;
    ssize_t data_size = 0;
    char *virt_addr = NULL;
    uintptr_t page_addr = 0;
    ssize_t pgno = pos / PAGESZ;
    uintptr_t dest_buff = (uintptr_t)buf;
    uintptr_t dest_end = (uintptr_t)buf + sz;

    if (!ip)
        return -EINVAL;
    
    if (ISDEV(ip))
        return kdev_read(_INODE_DEV(ip), pos, buf, sz);

    if (INODE_ISDIR(ip))
        return -EISDIR;

    CHK_IPTR(ip);

    if (!ip->ifs->fsuper->iops->read)
        return -ENOSYS;

    if (!(holding = mapping_holding(ip->mapping)))
        mapping_lock(ip->mapping);

    for (size_t i = 0; i < NPAGE(sz); ++i, ++pgno) {
        if ((pos >= ip->i_size))
        {
            data_size = -1;
            break;
        }

        if ((retval = mapping_get_page(ip->mapping, pgno, &page_addr, &page))) {
            if (!holding)
                mapping_unlock(ip->mapping);
            printk("error: %d\n", data_size);
            return data_size;
        }

        virt_addr = (char *)page->virtual;
        size = MIN(PAGESZ, (dest_end - dest_buff));
        size = MIN(size, (ip->i_size - pos));
        size = MIN((PAGESZ - PGOFFSET(pos)), size);
        memcpy((void *)dest_buff, (void *)(virt_addr + PGOFFSET(pos)), size);
        // printk("req: %d, read: %d, pos: %d, isize: %d\n", sz, size, pos, ip->i_size);
        pos += size;
        dest_buff += size;
        data_size += size;
    }

    if (!holding)
        mapping_unlock(ip->mapping);

    // printk("req: %d, read: %d, pos: %d, isize: %d\n", sz, data_size, pos, ip->i_size);
    return data_size;
    // data_size = ip->ifs->fsuper->iops->read(ip, pos, buf, sz);
}

size_t iwrite(inode_t *ip, off_t pos, void *buf, size_t sz)
{
    size_t size __unused = 0;
    int holding __unused = 0;
    ssize_t retval __unused = 0;
    page_t *page __unused = NULL;
    ssize_t data_size __unused = 0;
    char *virt_addr __unused = NULL;
    uintptr_t page_addr __unused = 0;
    ssize_t pgno __unused = pos / PAGESZ;
    uintptr_t src_buff __unused = (uintptr_t)buf;
    uintptr_t src_end __unused __unused = (uintptr_t)buf + sz;

    if (!ip)
        return -EINVAL;
    if (ISDEV(ip))
        return kdev_write(_INODE_DEV(ip), pos, buf, sz);
    
    if (INODE_ISDIR(ip))
        return -EISDIR;

    CHK_IPTR(ip);

    if (!ip->ifs->fsuper->iops->write)
        return -ENOSYS;

    if (!(holding = mapping_holding(ip->mapping)))
        mapping_lock(ip->mapping);

    for (size_t i = 0; i < NPAGE(sz); ++i, ++pgno)
    {
        if ((retval = mapping_get_page(ip->mapping, pgno, &page_addr, &page)))
        {
            if (!holding)
                mapping_unlock(ip->mapping);
            printk("error: %d\n", data_size);
            return data_size;
        }

        if (!page) { // write through to disk
            if ((size_t)(retval = ip->ifs->fsuper->iops->write(ip, pos, buf, sz)) != sz)
            {
                if (!holding)
                    mapping_unlock(ip->mapping);
                return retval;
            }

            if (!holding)
                mapping_unlock(ip->mapping);

            return retval;
        }

        virt_addr = (char *)page->virtual;
        size = MIN(PAGESZ, (src_end - src_buff));
        size = MIN(size, (ip->i_size - pos));
        size = MIN((PAGESZ - PGOFFSET(pos)), size);
        memcpy((void *)(virt_addr + PGOFFSET(pos)), (void *)src_buff, size);
        printk("req: %d, written: %d, pos: %d, isize: %d\n", sz, size, pos, ip->i_size);
        pos += size;
        src_buff += size;
        data_size += size;
    }
    
    if (!holding)
        mapping_unlock(ip->mapping);
    
    return data_size;
    return ip->ifs->fsuper->iops->write(ip, pos, buf, sz);
}

int iioctl(inode_t *ip, int req, void *argp)
{
    if (!ip)
        return -EINVAL;
    if (ISDEV(ip))
        return kdev_ioctl(_INODE_DEV(ip), req, argp);

    if (INODE_ISDIR(ip))
        return -ENOTTY;

    CHK_IPTR(ip);

    if (!ip->ifs->fsuper->iops->ioctl)
        return -ENOSYS;

    return ip->ifs->fsuper->iops->ioctl(ip, req, argp);
}

int ifind(inode_t *dir, const char *name, inode_t **ref)
{
    if (!dir || !name || !ref)
        return -EINVAL;

    CHK_IPTR(dir);

    if (!INODE_ISDIR(dir))
        return -ENOTDIR;

    if (!dir->ifs->fsuper->iops->find)
        return -ENOSYS;

    return dir->ifs->fsuper->iops->find(dir, name, ref);
}

int ibind(inode_t *dir, const char *name, inode_t *child)
{
    if (dir == NULL || name == NULL || child == NULL)
        return -EINVAL;

    CHK_IPTR(dir);

    if (!INODE_ISDIR(dir))
        return -ENOTDIR;

    if (!dir->ifs->fsuper->iops->bind)
        return -ENOSYS;

    return dir->ifs->fsuper->iops->bind(dir, name, child);
}

int imount(inode_t *dir, const char *name, inode_t *child)
{
    if (dir == NULL || name == NULL || child == NULL)
        return -EINVAL;

    CHK_IPTR(dir);

    if (!INODE_ISDIR(dir))
        return -ENOTDIR;

    if (!dir->ifs->fsuper->iops->mount)
        return -ENOSYS;

    return dir->ifs->fsuper->iops->mount(dir, name, child);
}

int ireaddir(inode_t *dir, off_t offset, struct dirent *dirent) {
    if (!dir || !dirent)
        return -EINVAL;

    CHK_IPTR(dir);

    if (!INODE_ISDIR(dir))
        return -ENOTDIR;

    if (!dir->ifs->fsuper->iops->readdir)
        return -ENOSYS;

    return dir->ifs->fsuper->iops->readdir(dir, offset, dirent);
}

/* check for file permission */
int iperm(inode_t *ip, uio_t *uio, int oflags)
{
    //printk("%s(\e[0;15mip=%p, uio=%p, oflags=%d)\e[0m\n", __func__, ip, uio, oflags);
    if (!ip || !uio)
        return -EINVAL;

    if (uio->u_uid == 0) /* root */
    {
        //printk("%s(): \e[0;12maccess granted, by virtue of being root\e[0m\n", __func__);
        return 0;
    }

    if (((oflags & O_ACCMODE) == O_RDONLY) || (oflags & O_ACCMODE) != O_WRONLY)
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
    if (((oflags & O_ACCMODE) == O_WRONLY) || (oflags & O_ACCMODE) == O_RDWR)
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
    if ((oflags & O_EXCL))
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

int ichown(inode_t *ip, uid_t uid, gid_t gid)
{
    if (ip == NULL)
        return -EINVAL;

    CHK_IPTR(ip);

    if (!ip->ifs->fsuper->iops->chown)
        return -ENOSYS;

    return ip->ifs->fsuper->iops->chown(ip, uid, gid);
}

int inode_getpage(inode_t *ip, ssize_t pgno, uintptr_t *ppaddr, page_t **ppage) {
    int err = 0;
    
    if (!ip)
        return -EINVAL;
    
    mapping_lock(ip->mapping);
    err = mapping_get_page(ip->mapping, pgno, ppaddr, ppage);
    mapping_unlock(ip->mapping);
    
    return err;
}

int icreate(inode_t *dir, dentry_t *dentry, mode_t mode)
{
    if (dir == NULL || dentry == NULL)
        return -EINVAL;

    CHK_IPTR(dir);

    if (!INODE_ISDIR(dir))
        return -ENOTDIR;

    if (!dir->ifs->fsuper->iops->creat)
        return -ENOSYS;

    return dir->ifs->fsuper->iops->creat(dir, dentry, mode);
}