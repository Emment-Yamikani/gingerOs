#include <lib/stdint.h>
#include <lib/string.h>
#include <lime/string.h>
#include <mm/kalloc.h>
#include <printk.h>
#include <fs/fs.h>
#include <sys/thread.h>
#include <fs/pipefs.h>
#include <bits/errno.h>

static int check_fd(int fd)
{
    if ((fd < 0) || (fd >= NFILE))
        return -EBADF;
    else
        return 0;
}

static int check_fildes(int fd, file_table_t *ft)
{
    file_table_assert(ft);
    file_table_assert_lock(ft);

    if ((fd < 0) || (fd >= NFILE) || !ft->file[fd])
        return -EBADF;
    else
        return 0;
}

static int fdalloc(struct file_table *table, file_t *file)
{
    int fd = 0;
    if (!table || !file)
        return -EINVAL;

    spin_assert_lock(table->lock);

    for (; fd < NFILE; ++fd)
    {
        if (!table->file[fd])
        {
            table->file[fd] = file;
            return fd;
        }
    }

    return -EMFILE;
}

static int file_alloc(file_t **ref)
{
    int err = 0;
    spinlock_t *lock = NULL;
    file_t *file = NULL;
    if (!ref)
        return -EINVAL;
    if ((err = spinlock_init(NULL, "file", &lock)))
        return err;
    if (!(file = kmalloc(sizeof *file)))
    {
        spinlock_free(lock);
        return -ENOMEM;
    }
    memset(file, 0, sizeof *file);
    file->f_lock = lock;
    file->f_ref = 1;
    *ref = file;
    return 0;
}

void file_free(file_t *file)
{
    if (file->f_lock)
        spinlock_free(file->f_lock);
    kfree(file);
}

static file_t *fileget(struct file_table *table, int fd)
{
    if (!table)
        return NULL;
    if ((fd > NFILE) || (fd < 0))
        return NULL;
    return table->file[fd];
}

int open(const char *fn, int oflags, ...)
{
    int err = 0, fd = 0;
    file_t *file = NULL;
    inode_t *inode = NULL;
    dentry_t *dentry = NULL;
    struct file_table *table = current->t_file_table;

    file_table_assert(table);

    if ((err = file_alloc(&file)))
        goto error;

    if ((err = vfs_lookup(fn, &table->uio, oflags, &inode, &dentry))) //@FIXME: provide process uio_t not 'NULL'
        goto error;

    file->f_inode = inode;
    file->f_flags = oflags;
    file->f_dentry = dentry;

    if ((err = fopen(file, oflags)))
        goto error;

    file_table_lock(table);
    if ((err = fd = fdalloc(table, file)) < 0)
    {
        file_table_unlock(table);
        goto error;
    }
    file_table_unlock(table);

    return fd;
error:
    printk("open(): called @ 0x%p, error=%d\n", return_address(0), err);
    return err;
}

size_t read(int fd, void *buf, size_t sz)
{
    size_t retval = 0;
    file_t *file = NULL;
    struct file_table *table = current->t_file_table;

    file_table_assert(table);
    file_table_lock(table);
    if ((retval = check_fildes(fd, table)))
    {
        file_table_unlock(table);
        return retval;
    }

    if (!(file = fileget(table, fd)))
    {
        file_table_unlock(table);
        return -EBADFD;
    }
    file_table_unlock(table);
    retval = fread(file, buf, sz);
    return retval;
}

size_t write(int fd, void *buf, size_t sz)
{
    size_t retval = 0;
    file_t *file = NULL;
    struct file_table *table = current->t_file_table;

    file_table_lock(table);
    if ((retval = check_fildes(fd, table)))
    {
        file_table_unlock(table);
        return retval;
    }

    if (!(file = fileget(table, fd)))
    {
        file_table_unlock(table);
        return -EBADFD;
    }
    file_table_unlock(table);
    retval = fwrite(file, buf, sz);
    return retval;
}

int pipe(int fildes[])
{
    int err = 0;
    int fd0 = 0, fd1 = 0;
    file_t *f0 = NULL, *f1 = NULL;
    struct file_table *table = current->t_file_table;

    if ((err = file_alloc(&f0)))
        goto error;
    if ((err = file_alloc(&f1)))
        goto error;

    file_table_assert(table);
    file_table_lock(table);

    if ((err = fd0 = fdalloc(table, f0)) < 0)
    {
        file_table_unlock(table);
        goto error;
    }

    if ((err = fd1 = fdalloc(table, f1)) < 0)
    {
        table->file[fd0] = NULL;
        file_table_unlock(table);
        goto error;
    }

    if ((err = pipefs_pipe(f0, f1)))
    {
        table->file[fd0] = NULL;
        table->file[fd1] = NULL;
        file_table_unlock(table);
        goto error;
    }

    file_table_unlock(table);

    fildes[0] = fd0;
    fildes[1] = fd1;

    return 0;
error:
    if (f0)
        file_free(f0);
    if (f1)
        file_free(f1);
    klog(KLOG_FAIL, "couldn't create pipe\n");
    return err;
}

off_t lseek(int fd, off_t offset, int whence)
{
    size_t retval = 0;
    file_t *file = NULL;
    struct file_table *table = current->t_file_table;
    file_table_assert(table);
    file_table_lock(table);
    if ((retval = check_fildes(fd, table)))
    {
        file_table_unlock(table);
        return retval;
    }

    if (!(file = fileget(table, fd)))
    {
        file_table_unlock(table);
        return -EBADFD;
    }
    file_table_unlock(table);

    retval = flseek(file, offset, whence);
    return retval;
}

off_t fstat(int fd, struct stat *buf)
{
    size_t retval = 0;
    file_t *file = NULL;
    struct file_table *table = current->t_file_table;
    file_table_assert(table);
    file_table_lock(table);
    if ((retval = check_fildes(fd, table)))
    {
        file_table_unlock(table);
        return retval;
    }

    if (!(file = fileget(table, fd)))
    {
        file_table_unlock(table);
        return -EBADFD;
    }
    file_table_unlock(table);
    retval = ffstat(file, buf);
    return retval;
}

int stat(const char *fn, struct stat *buf)
{
    size_t err = 0;
    inode_t *inode = NULL;
    file_table_lock(current->t_file_table);
    uio_t uio = current->t_file_table->uio;
    file_table_unlock(current->t_file_table);

    if ((err = vfs_open(fn, &uio, O_RDONLY | O_EXCL, &inode)))
        return err;
    if ((err = iperm(inode, &uio, O_RDONLY | O_EXCL)))
        return err;
    return istat(inode, buf);
}

int ioctl(int fd, int request, void *args /* args */)
{
    int err = 0;
    file_t *file = NULL;
    struct file_table *table = current->t_file_table;
    file_table_assert(table);
    file_table_lock(table);
    if ((err = check_fildes(fd, table)))
    {
        file_table_unlock(table);
        return err;
    }

    if (!(file = fileget(table, fd)))
    {
        file_table_unlock(table);
        return -EBADFD;
    }
    file_table_unlock(table);

    err = fioctl(file, request, args);

    return err;
}

int dup(int fd)
{
    int fd1 = 0, err = 0;
    file_t *file = NULL;
    struct file_table *table = current->t_file_table;

    file_table_assert(table);
    file_table_lock(table);
    if ((err = check_fildes(fd, table)))
    {
        file_table_unlock(table);
        return err;
    }

    if (!(file = fileget(table, fd)))
    {
        file_table_unlock(table);
        return -EBADFD;
    }

    if ((err = fd1 = fdalloc(table, file)) < 0)
    {
        file_table_unlock(table);
        return err;
    }

    if ((err = fdup(file)))
    {
        file_table_unlock(table);
        return err;
    }

    file_table_unlock(table);

    return fd1;
}

int dup2(int fd1, int fd2)
{
    int err = 0;
    file_t *f1 = NULL, *f2 = NULL;
    struct file_table *table = current->t_file_table;

    file_table_assert(table);
    file_table_lock(table);

    //klog(KLOG_WARN, "%s() not yet perfect\n", __func__);

    if ((err = check_fildes(fd1, table)))
    {
        file_table_unlock(table);
        return err;
    }

    if ((err = check_fd(fd2)))
    {
        file_table_unlock(table);
        return err;
    }

    if (!(f1 = fileget(table, fd1)))
    {
        file_table_unlock(table);
        return -EBADFD;
    }

    f2 = fileget(table, fd2);

    if (f1 == f2)
    {
        file_table_unlock(table);
        return fd2;
    }

    if (f2)
    {
        if ((err = fclose(f2)))
        {
            file_table_unlock(table);
            return err;
        }
    }

    if ((err = fdup(f1)))
    {
        file_table_unlock(table);
        return err;
    }

    table->file[fd2] = f1;
    file_table_unlock(table);

    return fd2;
}

int close(int fd)
{
    size_t retval = 0;
    file_t *file = NULL;
    struct file_table *table = current->t_file_table;

    file_table_assert(table);
    file_table_lock(table);

    if ((retval = check_fildes(fd, table)))
    {
        file_table_unlock(table);
        return retval;
    }

    if (!(file = fileget(table, fd)))
    {
        file_table_unlock(table);
        return -EBADFD;
    }

    retval = fclose(file);
    table->file[fd] = NULL;
    file_table_unlock(table);

    return retval;
}

int chdir(char *fn)
{
    int err = 0;
    char *dir = NULL;
    inode_t *inode = NULL;
    file_table_lock(current->t_file_table);
    uio_t uio = current->t_file_table->uio;

    if ((err = vfs_open(fn, &uio, O_RDONLY | O_EXCL, &inode)))
    {
        file_table_unlock(current->t_file_table);
        return err;
    }
    if ((err = iperm(inode, &uio, O_RDONLY | O_EXCL)))
    {
        file_table_unlock(current->t_file_table);
        return err;
    }
    if (!(dir = strdup(fn)))
    {
        file_table_unlock(current->t_file_table);
        return -ENOMEM;
    }
    if (uio.u_cwd)
        kfree(uio.u_cwd);
    current->t_file_table->uio.u_cwd = dir;
    file_table_unlock(current->t_file_table);
    return 0;
}

char *getcwd(char *buf, size_t sz)
{
    file_table_lock(current->t_file_table);
    uio_t uio = current->t_file_table->uio;
    if (((int)sz < (strlen(uio.u_cwd) + 1)))
    {
        file_table_unlock(current->t_file_table);
        return NULL;
    }
    if (uio.u_cwd)
        strncpy(buf, uio.u_cwd, sz);
    file_table_unlock(current->t_file_table);
    return buf;
}