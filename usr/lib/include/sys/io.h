#ifndef IO_H
# define IO_H

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#ifdef __cplusplus
extern "C"
{
#endif

    extern int mkdir(const char *);
    extern char *mktemp(char *);
    extern int rmdir(const char *);
    extern int chmod(const char *, int);
    extern int remove(const char *);
    extern int rename(const char *, const char *);

    extern int open(const char *path, int mode, ...);
    extern int write(int fd, void *buf, unsigned int size);
    extern int read(int fd, void *__buf, unsigned int size);
    extern int creat(const char *path, int mode);
    extern int lseek(int fd, long off, int whence);
    extern int close(int fd);
    extern int dup(int fd);
    extern int dup2(int fd1, int fd2);
    extern int chdir(char *dir);
    extern char *getcwd(char *__buf, long __size);

#ifdef __cplusplus
}
#endif

#endif