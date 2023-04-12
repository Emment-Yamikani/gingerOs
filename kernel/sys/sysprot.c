#include <bits/errno.h>
#include <fs/fs.h>
#include <lib/types.h>
#include <lime/assert.h>
#include <printk.h>
#include <sys/thread.h>

uid_t getuid(void)
{
    file_table_t *table = NULL;
    current_assert();
    table = current->t_file_table;
    file_table_assert(table);
    file_table_lock(table);
    uid_t uid = table->uio.u_uid;
    file_table_unlock(table);
    return uid;
}

gid_t getgid(void)
{
    file_table_t *table = NULL;
    current_assert();
    table = current->t_file_table;
    file_table_assert(table);
    file_table_lock(table);
    uid_t gid = table->uio.u_gid;
    file_table_unlock(table);
    return gid;
}

int setuid(uid_t uid)
{
    file_table_t *table = NULL;
    current_assert();
    if (uid < 0) return -EINVAL;

    table = current->t_file_table;
    file_table_assert(table);
    file_table_lock(table);
    uid_t real_uid = table->uio.u_uid;

    if(real_uid == 0) // root
    {
        table->uio.u_uid = uid;
        table->uio.u_euid = uid;
        file_table_unlock(table);
        return 0;
    }
    else if (real_uid == uid)
    {
        table->uio.u_euid = uid;
        file_table_unlock(table);
        return 0;
    }

    file_table_unlock(table);
    return -EPERM;
}

int setgid(gid_t gid)
{
    file_table_t *table = NULL;
    current_assert();
    if (gid < 0) return -EINVAL;

    table = current->t_file_table;
    file_table_assert(table);
    file_table_lock(table);
    uid_t real_gid = table->uio.u_gid;

    if(real_gid == 0) // root
    {
        table->uio.u_gid = gid;
        table->uio.u_egid = gid;
        file_table_unlock(table);
        return 0;
    }
    else if (real_gid == gid)
    {
        table->uio.u_egid = gid;
        file_table_unlock(table);
        return 0;
    }

    file_table_unlock(table);
    return -EPERM;
}

int chown(const char *pathname, uid_t uid, gid_t gid)
{
    int err = 0;
    uio_t uio = {0};
    inode_t *file = NULL;
    file_table_t *table = NULL;

    if (pathname == NULL)
        return -EINVAL;

    current_lock();
    table = current->t_file_table;
    current_unlock();

    file_table_lock(table);
    uio = table->uio;

    if ((err = vfs_open(pathname, &uio, O_RDWR, 0, &file)))
    {
        file_table_unlock(table);
        goto error;
    }
    
    file_table_unlock(table);

    if ((err = ichown(file, uid, gid)))
        goto error;

    return 0;
error:
    klog(KLOG_FAIL, "failed to change \'%s\' owner, error: %d\n", pathname, err);
    return err;
}

int fchown(int fd, uid_t uid, gid_t gid)
{
    size_t retval = 0;
    file_t *file = NULL;
    inode_t *inode = NULL;
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

    inode = file->f_inode;
    //TODO: deligate this operation to filesystem
    return ichown(inode, uid, gid);
}