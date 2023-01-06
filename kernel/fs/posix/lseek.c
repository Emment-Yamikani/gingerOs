#include <fs/fs.h>
/**
 * posix_file_lseek
 *
 * Conforming to `IEEE Std 1003.1, 2013 Edition'
 *
 */

long posix_file_lseek(struct file *file, long offset, int whence)
{
    struct inode *inode = file->f_dentry->d_inode;

    switch (whence) {
        case 0: /* SEEK_SET */
            file->f_pos = offset;
            break;
        case 1: /* SEEK_CUR */
            file->f_pos += offset;
            break;
        case 2: /* SEEK_END */
            file->f_pos = inode->i_size + offset;
            break;
    }

    return file->f_pos;
}
