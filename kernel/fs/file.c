#include <lib/stdint.h>
#include <lib/string.h>
#include <lime/string.h>
#include <mm/kalloc.h>
#include <printk.h>
#include <fs/fs.h>
#include <sys/thread.h>
#include <bits/errno.h>
#include <dev/dev.h>

#define CHK_FPTR(file)                     \
{                                          \
    if (!file->f_inode->ifs)               \
        return -ENOSYS;                    \
    if (!file->f_inode->ifs->fsuper)       \
        return -ENOSYS;                    \
    if (!file->f_inode->ifs->fsuper->fops) \
        return -ENOSYS;                    \
}

void f_free_table(struct file_table *ftable)
{
    if (ftable->lock) spinlock_free(ftable->lock);
    kfree(ftable);
}

int f_alloc_table(struct file_table **rft)
{
    int err =0;
    spinlock_t *lk = NULL;
    file_table_t *ft = NULL;
    assert(rft, "no file table reference pointer");
    if (!(ft = kmalloc(sizeof *ft)))
        return -ENOMEM;
    if ((err = spinlock_init(NULL, "file_table", &lk)))
        goto error;
    memset(ft, 0, sizeof *ft);
    ft->lock = lk;
    *rft = ft;
    return 0;
error:
    if (ft) kfree(ft);
    if (lk) spinlock_free(lk);
    klog(KLOG_FAIL, "couldn't allocate a file description table, error: %d\n", err);
    return err;
}

int fdup(file_t *file)
{
    if (!file)
        return -EINVAL;

    flock(file);
    file->f_ref++;
    funlock(file);
    //printk("file->refs: %d\n", file->f_ref);
    return 0;
}

int fcan_read(struct file *file, size_t size)
{
    if (!file || !file->f_inode)
        return -EINVAL;

    if (ISDEV(file->f_inode))
        return kdev_fcan_read(_INODE_DEV(file->f_inode), file, size);

    CHK_FPTR(file);

    if (!file->f_inode->ifs->fsuper->fops->can_read)
        return -ENOSYS;

    return file->f_inode->ifs->fsuper->fops->can_read(file, size);
}

int fcan_write(struct file *file, size_t size)
{
    if (!file || !file->f_inode)
        return -EINVAL;

    if (ISDEV(file->f_inode))
        return kdev_fcan_write(_INODE_DEV(file->f_inode), file, size);
    
   CHK_FPTR(file);

    if (!file->f_inode->ifs->fsuper->fops->can_write)
        return -ENOSYS;

    return file->f_inode->ifs->fsuper->fops->can_write(file, size);
}

int freaddir(file_t *file, struct dirent *dirent){
    if (!file || !file->f_inode)
        return -EINVAL;

    if (ISDEV(file->f_inode))
        return -ENOTDIR;

    CHK_FPTR(file);

    if (!file->f_inode->ifs->fsuper->fops->readdir)
        return -ENOSYS;

    return file->f_inode->ifs->fsuper->fops->readdir(file, dirent);
}

size_t feof(struct file *file)
{
    if (!file || !file->f_inode)
        return -EINVAL;

    if (ISDEV(file->f_inode))
        return kdev_feof(_INODE_DEV(file->f_inode), file);

   CHK_FPTR(file);

    if (!file->f_inode->ifs->fsuper->fops->eof)
        return -ENOSYS;

    return file->f_inode->ifs->fsuper->fops->eof(file);
}

int fopen(file_t *file, int oflags, ...)
{
    if (!file || !file->f_inode)
        return -EINVAL;
    
    if (ISDEV(file->f_inode))
        return kdev_fopen(_INODE_DEV(file->f_inode), file, oflags);

   CHK_FPTR(file);
    
    if (!file->f_inode->ifs->fsuper->fops->open)
        return -ENOSYS;
    
    return file->f_inode->ifs->fsuper->fops->open(file, oflags);
}

int fread(file_t *file, void *buf, size_t sz)
{
    if (!file || !file->f_inode)
        return -EINVAL;
    
    if (ISDEV(file->f_inode))
        return kdev_fread(_INODE_DEV(file->f_inode), file, buf, sz);

    CHK_FPTR(file);
    
    if (!file->f_inode->ifs->fsuper->fops->read)
        return -ENOSYS;
    
    return file->f_inode->ifs->fsuper->fops->read(file, buf, sz);
}

int fwrite(file_t *file, void *buf, size_t sz)
{
    if (!file || !file->f_inode)
        return -EINVAL;
    
    if (ISDEV(file->f_inode))
        return kdev_fwrite(_INODE_DEV(file->f_inode), file, buf, sz);

    CHK_FPTR(file);

    if (!file->f_inode->ifs->fsuper->fops->write)
        return -ENOSYS;

    return file->f_inode->ifs->fsuper->fops->write(file, buf, sz);
}

off_t flseek(file_t *file, off_t offset, int whence)
{
    if (!file || !file->f_inode)
        return -EINVAL;
    
    if (ISDEV(file->f_inode))
        return kdev_flseek(_INODE_DEV(file->f_inode), file, offset, whence);

    CHK_FPTR(file);

    if (!file->f_inode->ifs->fsuper->fops->lseek)
        return -ENOSYS;

    return file->f_inode->ifs->fsuper->fops->lseek(file, offset, whence);
}

int ffstat(file_t *file, struct stat *buf)
{
    if (!file || !file->f_inode)
        return -EINVAL;

    if (ISDEV(file->f_inode))
        return kdev_fstat(_INODE_DEV(file->f_inode), file, buf);

    CHK_FPTR(file);

    if (!file->f_inode->ifs->fsuper->fops->stat)
        return -ENOSYS;

    return file->f_inode->ifs->fsuper->fops->stat(file, buf);
}

int fclose(file_t *file)
{
    if (!file || !file->f_inode)
        return -EINVAL;
    
    if (ISDEV(file->f_inode))
        return kdev_fclose(_INODE_DEV(file->f_inode), file);

    CHK_FPTR(file);

    if (!file->f_inode->ifs->fsuper->fops->close)
        return -ENOSYS;

    return file->f_inode->ifs->fsuper->fops->close(file);
}

int fioctl(file_t *file, int request, void *args/* args */)
{
    if (!file || !file->f_inode)
        return -EINVAL;
    
    if (ISDEV(file->f_inode))
        return kdev_fioctl(_INODE_DEV(file->f_inode), file, request, args/* args */);

    CHK_FPTR(file);
    
    if (!file->f_inode->ifs->fsuper->fops->ioctl)
        return -ENOSYS;
    
    return file->f_inode->ifs->fsuper->fops->ioctl(file, request, args);
}