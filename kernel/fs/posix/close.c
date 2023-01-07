#include <fs/posix.h>

int posix_file_close(struct file *file)
{
    if ((--file->f_ref) > 0)
        return 0;
    
    file_free(file);
    int err = iclose(file->f_inode);
    return err;
}
