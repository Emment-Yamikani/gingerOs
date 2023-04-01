#ifndef UNISTD_H
# define UNISTD_H

#include "sys/io.h"
#include "sys/stat.h"
#include "types.h"

#ifdef __cplusplus
extern "C"
{
#endif

///syscalls
    extern int sysputchar(int c);
    
    extern int open(const char *path, int mode, ...);
    extern int write(int fd, void *buf, unsigned int size);
    extern int read(int fd, void *__buf, unsigned int size);
    extern int fstat(int fd, struct stat *buf);
    extern int creat(const char *path, int mode);
    extern int stat(const char *path, struct stat *buf);
    extern int lseek(int fd, long off, int whence);
    extern int close(int fd);
    extern int dup(int fd);
    extern int dup2(int fd1, int fd2);
    extern int chdir(char *dir);
    extern char *getcwd(char *__buf, long __size);

    extern int brk(void *addr);
    extern void *sbrk(int nbytes);
    extern int getpagesize(void);

    extern gid_t getgid();
    extern uid_t getuid();
    extern gid_t setgid(gid_t gid);
    extern uid_t setuid(uid_t uid);
    extern int chown(const char *, uid_t, gid_t);
    extern int fchown(int , uid_t, gid_t);

    extern int pipe(int *p);
    
    extern int fork();
    extern int getpid(void);
    extern int getppid(void);
    extern long sleep(long ms);
    extern void exit(int status);
    extern int wait(int *stat_loc);
    extern int execv(char *path, char **argp);
    extern int execve(char *path, char *const argp[], char *const envp[]);
    
    extern void thread_yield(void);
    extern int thread_create(tid_t *tid, void *(*func)(void *), void *arg);
    extern int thread_self(void);
    extern int thread_join(tid_t __tid, void *__res);
    extern void thread_exit(void *_exit);
    extern int park(void);
    extern int unpark(tid_t tid);
    
    extern pid_t getpgrp(void);
    extern pid_t getpgid(pid_t pid);
    extern pid_t setpgrp(void);
    extern int setpgid(pid_t pid, pid_t pgid);
    extern pid_t getsid(pid_t pid);
    extern pid_t setsid(void);
    
    extern void (*signal(int sig, void (*handler)(int)))(int);
    extern int kill(pid_t pid, int sig);
    extern int pause(void);
    
    extern int openpt(int flags);
    extern int grantpt(int fd);
    extern int ptsname_r(int fd, char *buf, long buflen);
    extern char *ptsname(int fd);
    extern int isatty(int fd);
    extern int unlockpt(int fd);

#ifdef __cplusplus
}
#endif

#endif