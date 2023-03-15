#include <fs/posix.h>
#include <lib/stddef.h>

size_t posix_file_can_read(struct file *file, size_t size)
{
    return (size_t) (file->f_pos + size < file->f_inode->i_size);
}

size_t posix_file_can_write(struct file *file, size_t size)
{
    return (size_t) (file->f_pos + size < file->f_inode->i_size);
}

size_t posix_file_eof(struct file *file)
{
    return (size_t) (file->f_pos >= file->f_inode->i_size);
}

struct fops posix_fops = {
    .close = posix_file_close,
    .eof = posix_file_eof,
    .ioctl = posix_file_ioctl,
    .lseek = posix_file_lseek,
    .open = posix_file_open,
    .perm = NULL,
    .read = posix_file_read,
    .readdir = posix_file_readdir,
    .sync = NULL,
    .mmap = posix_file_mmap,
    .stat = posix_file_ffstat,
    .write = posix_file_write,
    .can_read = posix_file_can_read,
    .can_write = posix_file_can_write,
};