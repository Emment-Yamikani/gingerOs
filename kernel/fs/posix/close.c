#include <fs/posix.h>

int posix_file_close(struct file *file)
{
    return vfs_fclose(file);
}
