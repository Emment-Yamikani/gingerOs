#include <arch/i386/cpu.h>
#include <bits/dirent.h>
#include <bits/errno.h>
#include <fs/fs.h>
#include <sys/proc.h>
#include <sys/thread.h>
#include <fs/sysfile.h>

int readdir(int fd, struct dirent *dirent){
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
    
    retval = freaddir(file, dirent);

    file_table_unlock(table);
    return retval;
}