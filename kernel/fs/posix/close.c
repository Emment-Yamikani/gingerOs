#include <fs/posix.h>

int posix_file_close(struct file *file)
{
    int err = iclose(file->f_inode);
    return err;
}
