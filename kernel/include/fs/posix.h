#ifndef __VFS_POSIX
#define __VFS_POSIX

#include <fs/fs.h>
#include <lib/stddef.h>

int     posix_file_open(struct file *file);
int     posix_file_close(struct file *file);
size_t    posix_file_read(struct file *file, char *buf, size_t size);
size_t    posix_file_write(struct file *file, char *buf, size_t size);

//size_t posix_file_readdir(struct file *file, struct dirent *dirnet);

int     posix_file_ioctl(struct file *file, int request, void *argp);
size_t    posix_file_lseek(struct file *file, size_t offset, int whence);

/* helpers */
size_t posix_file_eof(struct file *file);
size_t posix_file_can_read(struct file *file, size_t size);
size_t posix_file_can_write(struct file *file, size_t size);

extern struct fops posix_fops;

#endif /* ! __VFS_POSIX */