#include <lib/stdint.h>
#include <lib/string.h>
#include <lime/string.h>
#include <mm/kalloc.h>
#include <printk.h>
#include <fs/fs.h>
#include <sys/thread.h>
#include <bits/errno.h>
#include <dev/dev.h>

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

    return 0;
}

int fopen(file_t *file, int oflags)
{
    if (!file)
        return -EINVAL;
   return iopen(file->f_inode, oflags);
}

int fread(file_t *file, void *buf, size_t sz)
{
    if (!file)
        return -EINVAL;
    return iread(file->f_inode, file->f_pos, buf, sz);
}

int fwrite(file_t *file, void *buf, size_t sz)
{
    if (!file)
        return -EINVAL;
    return iwrite(file->f_inode, file->f_pos, buf, sz);
}

off_t flseek(file_t *file, off_t offset, int whence)
{
    if (!file)  return -EINVAL;
    if (ISDEV(file->f_inode))
        return kdev_lseek(_INODE_DEV(file->f_inode), offset, whence);
    switch (whence)
    {
    case 0: /* SEEK_SET */
        file->f_pos = offset;
        break;
    case 1: /* SEEK_CUR */
        file->f_pos += offset;
        break;
    case 2: /* SEEK_END */
        file->f_pos = file->f_inode->i_size + offset;
        break;
    }
    return file->f_pos;
}

int ffstat(file_t *file, struct stat *buf)
{
    if (!file) return -EINVAL;
    return istat(file->f_inode, buf);
}

void file_free(file_t *file);
int fclose(file_t *file)
{
    if (!file) return -EINVAL;
    flock(file);
    file->f_ref--;
    if (file->f_ref > 0)
    {
        funlock(file);
        return 0;
    }
    iclose(file->f_inode);
    file_free(file);
    return 0;
}

int fioctl(file_t *file, int request, void *args/* args */)
{
    if (!file) return -EINVAL;
    return iioctl(file->f_inode, request, args);
}