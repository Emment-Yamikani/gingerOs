#include <fs/fs.h>
#include <bits/errno.h>

/**
 * posix_file_ioctl
 *
 * Conforming to `IEEE Std 1003.1, 2013 Edition'
 *
 */

int posix_file_ioctl(struct file *file, int request, void *argp)
{
    int retval =0;

    if (file == NULL)
        return -EINVAL;

    flock(file);
    if (file->f_inode == NULL)
    {
        funlock(file);
        return -EINVAL;
    }

    retval = iioctl(file->f_inode, request, argp);

    funlock(file);
    return retval;
}
