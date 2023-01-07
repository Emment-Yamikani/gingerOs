#include <fs/posix.h>
#include <fs/fs.h>
#include <bits/errno.h>
#include <lib/stddef.h>

int posix_file_ffstat(struct file *file, struct stat *buf)
{
    int retval = 0;
    if (!file || !file->f_inode)
        return -EINVAL;

    retval = istat(file->f_inode, buf);

    return retval;
}