#include <fs/fs.h>
/**
 * posix_file_lseek
 *
 * Conforming to `IEEE Std 1003.1, 2013 Edition'
 *
 */

off_t posix_file_lseek(struct file *file, off_t offset, int whence)
{
    size_t pos = 0;
    struct inode *inode = NULL;
    

    flock(file);

    inode = file->f_inode;
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
    pos = file->f_pos;

    funlock(file);
    return pos;
}
