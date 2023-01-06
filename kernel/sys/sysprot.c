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
