#include <dev/pty.h>
#include <fs/fs.h>
#include <printk.h>
#include <lib/string.h>
#include <bits/errno.h>
#include <sys/thread.h>
#include <sys/proc.h>
#include <arch/i386/cpu.h>
#include <dev/dev.h>

/*Pseudo terminals*/

int isatty(int fd)
{
    int err = 0;
    file_t *file = NULL;
    struct file_table *table = current->t_file_table;

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

    flock(file);
    ilock(file->f_inode);

    if (!ISDEV(file->f_inode))
    {
        iunlock(file->f_inode);
        funlock(file);
        return -ENOTTY;
    }

    if ((file->f_inode->i_rdev & _DEV_T(DEV_PTS, 0)) ||
        (file->f_inode->i_rdev & _DEV_T(DEV_PTMX, 0)))
    {
        iunlock(file->f_inode);
        funlock(file);
        return 1;
    }

    iunlock(file->f_inode);
    funlock(file);
    return -ENOTTY;
}

int grantpt(int fd)
{
    uio_t uio;
    int err = 0;
    PTY pty = NULL;
    file_t *file = NULL;
    inode_t *slave = NULL;
    struct file_table *table = current->t_file_table;

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


    flock(file);
    ilock(file->f_inode);

    if (!ISDEV(file->f_inode))
    {
        iunlock(file->f_inode);
        funlock(file);
        file_table_unlock(table);
        return -ENOTTY;
    }

    if (!(file->f_inode->i_rdev & _DEV_T(DEV_PTS, 0)) &&
        !(file->f_inode->i_rdev & _DEV_T(DEV_PTMX, 0)))
    {
        iunlock(file->f_inode);
        funlock(file);
        file_table_unlock(table);
        return -ENOTTY;
    }

    uio = table->uio;
    pty = (PTY)file->f_inode->i_priv;
    slave = pty->slave;

    ilock(slave);
    slave->i_gid = uio.u_gid;
    slave->i_uid = uio.u_uid;
    slave->i_mask = 0620;
    iunlock(slave);

    iunlock(file->f_inode);
    funlock(file);
    file_table_unlock(table);
    return 0;
}

int openpt(int flags)
{
    (void)flags;
    return -ENOSYS;
}

int unlockpt(int fd)
{
    int err = 0;
    file_t *file = NULL;
    struct file_table *table = current->t_file_table;

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

    flock(file);
    ilock(file->f_inode);

    if (((file->f_flags & O_WRONLY) == 0) && ((file->f_flags & O_RDWR) == 0))
    {
        iunlock(file->f_inode);
        funlock(file);
        return -EBADF;
    }

    if (!ISDEV(file->f_inode))
    {
        iunlock(file->f_inode);
        funlock(file);
        return -ENOTTY;
    }

    if ((file->f_inode->i_rdev & _DEV_T(DEV_PTMX, 0)) == 0)
    {
        iunlock(file->f_inode);
        funlock(file);
        return -EINVAL;
    }

    ((PTY)file->f_inode->i_priv)->slave_unlocked = 1;

    iunlock(file->f_inode);
    funlock(file);
    return 0;
}

int ptsname_r(int fd, char *buf, size_t buflen)
{
    int err = 0;
    file_t *file = NULL;
    struct file_table *table = current->t_file_table;

    if (buf == NULL)
        return -EINVAL;
    

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

    flock(file);
    ilock(file->f_inode);

    if (!ISDEV(file->f_inode))
    {
        iunlock(file->f_inode);
        funlock(file);
        return -ENOTTY;
    }

    if (((file->f_inode->i_rdev & _DEV_T(DEV_PTMX, 0)) == 0)/*&&
        ((file->f_inode->i_rdev & _DEV_T(DEV_PTS, 0)) == 0)*/)
    {
        iunlock(file->f_inode);
        funlock(file);
        return -ENOTTY;
    }

    dlock(file->f_dentry);

    char path[] = "/dev/pts/";

    size_t namelen = sizeof path + strlen(file->f_dentry->d_name);

    if (namelen > buflen)
    {
        dunlock(file->f_dentry);
        iunlock(file->f_inode);
        funlock(file);
        return -ERANGE;
    }

    strncpy(buf, path, (sizeof path - 1));
    strncpy(buf + (sizeof path - 1), file->f_dentry->d_name, strlen(file->f_dentry->d_name));
    buf[namelen - 1] = '\0';

    dunlock(file->f_dentry);
    iunlock(file->f_inode);
    funlock(file);
    return 0;
}
