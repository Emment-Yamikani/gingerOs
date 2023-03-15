#include <fs/posix.h>
#include <bits/errno.h>
#include <fs/fs.h>

int posix_file_mmap(file_t *file, vmr_t *vmr)
{
    (void)file;
    (void)vmr;
    return -ENOSYS;
}