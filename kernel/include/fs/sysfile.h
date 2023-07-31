#ifndef FS_SYSFILE_H
#define FS_SYSFILE_H 1

#include <lib/stddef.h>
#include <lib/stdint.h>
#include <sys/_stat.h>
#include <lib/types.h>
#include <bits/dirent.h>

int pipe(int []);
int open(const char *fn, int oflags, mode_t mode);
size_t read(int fd, void *buf, size_t sz);
size_t write(int fd, void *buf, size_t sz);

int readdir(int fd, struct dirent *dirent);

/* seek */
off_t lseek(int fd, off_t offset, int whence);

/* file stats using fildes*/
int fstat(int fd, struct stat *buf);

/* file stats using filename*/
int stat(const char *fn, struct stat *buf);

/* io control */
int ioctl(int fd, int request, ... /* args */);

/* duplicate fildes */
int dup(int fd);

/* deplicate fildes to fildes2 */
int dup2(int fd, int fd2);

/* close file */
int close(int fd);

/* change current working directory */
int chdir(char *fn);

/* get current working directory */
char *getcwd(char *buf, size_t nbytes);

#endif // FS_SYSFILE_H