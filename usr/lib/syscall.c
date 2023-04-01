/* system API numbers */

/* Miscelleneous. */

#define SYS_KPUTC           0 //* put a character to the kernel's terminal

/* Process management */

#define SYS_EXECVE          1 //* execve
#define SYS_FORK            2 //* fork a child
#define SYS_YIELD           3 //* yield cpu
#define SYS_EXIT            4 //* exit the process
#define SYS_WAIT            5 //* wait for child to terminate
#define SYS_SLEEP           6 //* sleep for n secs.
#define SYS_GETPID          7 //* get process ID
#define SYS_EXECV           8 //* exec
#define SYS_GETPPID         9 //* get process' parent ID

/* Protection */

#define SYS_SETGID          10 //* set real group ID
#define SYS_SETUID          11 //* set real user ID
#define SYS_GETGID          12 //* get real group ID
#define SYS_GETUID          13 //* set real user ID

/* Standard IO */

#define SYS_CREAT	        14 //* create a file
#define SYS_STAT	        15 //* get file stats.
#define SYS_FSTAT           16 //* get file stats.
#define SYS_LSEEK           17 //* seek in file
#define SYS_WRITE	        18 //* write to file
#define SYS_CLOSE	        19 //* close file
#define SYS_PIPE            20 //* open an unamed pipe
#define SYS_OPEN	        21 //* open file
#define SYS_READ	        22 //* read from file
#define SYS_CHDIR           23 //* change working directory
#define SYS_GETCWD          24 //* get current working directory
#define SYS_DUP             25 //* duplicate fildes
#define SYS_DUP2            26 //* duplicate fildes
#define SYS_IOCTL           27 //* ioctl

/* Memory management */

#define SYS_SBRK	        28 //* sbrk
#define SYS_BRK             29
#define SYS_GETPAGESIZE     30 //* get kernel page size (currently not dynamic)
#define SYS_MMAP            31

/* Signals */

#define SYS_KILL            32 //* send signal
#define SYS_SIGNAL          33 //* set signal handler
#define SYS_PAUSE           34 //* wait for a signal

#define SYS_THREAD_CREATE   35 //* create a thread
#define SYS_THREAD_SELF     36 //* get thread ID
#define SYS_THREAD_JOIN     37 //* wait for thread TID to terminate
#define SYS_THREAD_EXIT     38 //* terminate thread
#define SYS_THREAD_YIELD    39 //* create a thread

#define SYS_GETPGRP         40 //* get process group ID
#define SYS_SETPGRP         41 //* set process group ID
#define SYS_SETSID          42 //* set session ID
#define SYS_GETSID          43 //* get session ID
#define SYS_GETPGID         44 //* get process group ID of process PID
#define SYS_SETPGID         45 //* set process group ID of process PID

/* Terminals & Pseudo-terminals */

#define SYS_OPENPT          46 //* open pseudo-terminal
#define SYS_GRANTPT         47 //* grant pseudo-terminal permissions
#define SYS_PTSNAME_R       48 //* get pseudo-terminal name
#define SYS_ISATTY          49 //* isatty?
#define SYS_UNLOCKPT        50 //* unlock a pty pair

/*System configurations*/

#define SYS_MUNMAP         51

/*Directory manipulation*/

#define SYS_READDIR         52
#define SYS_CHOWN           53
#define SYS_FCHOWN          54

#define SYS_PARK            57
#define SYS_UNPARK          58

/*
#define SYSCALL5(ret, v, arg1, arg2, arg3, arg4, arg5) \
	asm volatile("int $0x80;":"=a"(ret):"a"(v), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4), "D"(arg5));
#define SYSCALL4(ret, v, arg1, arg2, arg3, arg4) \
	asm volatile("int $0x80;":"=a"(ret):"a"(v), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4));
#define SYSCALL3(ret, v, arg1, arg2, arg3) \
	asm volatile("int $0x80;":"=a"(ret):"a"(v), "b"(arg1), "c"(arg2), "d"(arg3));
#define SYSCALL2(ret, v, arg1, arg2) \
	asm volatile("int $0x80;":"=a"(ret):"a"(v), "b"(arg1), "c"(arg2));
#define SYSCALL1(ret, v, arg1) \
	asm volatile("int $0x80;":"=a"(ret):"a"(v), "b"(arg1));
#define SYSCALL0(ret, v) \
	asm volatile("int $0x80;":"=a"(ret):"a"(v));
*/

#include <sys/stat.h>
#include <types.h>
#include <stdint.h>
#include <stddef.h>

extern int sys_kputc(int c);

extern void *sys_mmap(void *addr, size_t length, int prot, int flags,
           int fd, off_t offset);
extern int sys_munmap(void *addr, size_t length);
int sys_mprotect(void *addr, size_t len, int prot);
extern int sys_brk(void *addr);
extern void *sys_sbrk(int nbytes);
extern int sys_getpagesize(void);

extern int sys_open(const char *path, int mode, ...);
extern int sys_read(int fd, void *__buf, unsigned int size);
extern int sys_lseek(int fd, long off, int whence);
extern int sys_write(int fd, void *buf, unsigned int size);
extern int sys_close(int fd);
extern char *sys_getcwd(char *__buf, long __size);
extern int sys_chdir(char *dir);
extern int sys_dup(int fd);
extern int sys_dup2(int fd1, int fd2);
extern int sys_ioctl(int fd, long request, void *arg);
extern int sys_fstat(int fd, struct stat*buf);
extern int sys_pipe(int *p);
extern int sys_stat(const char *path, struct stat *buf);
extern int sys_creat(const char *path, int mode);

extern int sys_getuid();
extern int sys_getgid();
extern int sys_setgid(int gid);
extern int sys_setuid(int uid);
extern int sys_chown(const char *, uid_t, gid_t);
extern int sys_fchown(int, uid_t, gid_t);


extern int sys_getpid(void);
extern int sys_getppid(void);
extern int sys_execve(char *path, char *const argp[], char *const envp[]);
extern int sys_execv(char *path, char **argp);
extern int sys_fork();
extern void sys_exit(int status);
extern int sys_wait(int *stat_loc);
extern long sys_sleep(long ms);


extern int sys_thread_create(tid_t *tid, void *(*func)(void *), void *arg);
extern void sys_thread_yield(void);
extern int sys_thread_self(void);
extern int sys_thread_join(tid_t __tid, void *__res);
extern void sys_thread_exit(void *_exit);
extern int sys_thread_cancel();
extern int sys_park();
extern int sys_unpark(tid_t __tid);

extern pid_t sys_getpgrp(void);
extern pid_t sys_getpgid(pid_t pid);
extern pid_t sys_setpgrp(void);
extern int sys_setpgid(pid_t pid, pid_t pgid);
extern pid_t sys_getsid(pid_t pid);
extern pid_t sys_setsid(void);

extern void (*sys_signal(int sig, void (*handler)(int)))(int);
extern int sys_kill(pid_t pid, int sig);
extern int sys_pause(void);

extern int sys_openpt(int flags);
extern int sys_grantpt(int fd);
extern int sys_ptsname_r(int fd, char *buf, long buflen);
extern char *sys_ptsname(int fd);
extern int sys_isatty(int fd);
extern int sys_unlockpt(int fd);


int getpid(void)
{
    return sys_getpid();
}

int getppid(void)
{
    return sys_getppid();
}

int sysputchar(int c)
{
    return sys_kputc(c);
}

int open(const char *path, int mode, ...)
{
    return sys_open(path, mode);
}

int read(int fd, void *__buf, unsigned int size)
{
    return sys_read(fd, __buf, size);
}

int getuid()
{
    return sys_getuid();
}

extern int dprintf(int, char *, ...);

int brk(void *addr)
{
    (void)addr;
    //return sys_brk(addr);
    return -1;
}

void *sbrk(int nbytes)
{
    return (void *)sys_sbrk(nbytes);
}

int getpagesize(void)
{
    return sys_getpagesize();
}

int lseek(int fd, long off, int whence)
{
    return sys_lseek(fd, off, whence);
}

int write(int fd, void *buf, unsigned int size)
{
    return sys_write(fd, buf, size);
}

int close(int fd)
{
    return sys_close(fd);
}

char *getcwd(char *__buf, long __size)
{
    char *ret;
    ret = sys_getcwd(__buf, __size);
    if (!ret)
        return 0;
    int len = 0;
	while (__buf[len])
		len++;
    if (len > 1)
        if (__buf[--len] == '/')
            __buf[len] = '\0';
    return ret;
}

int chdir(char *dir)
{
    int ret, len = 0;
    while (dir[len])
        len++;
    if (len > 1)
        if (dir[--len] == '/')
            dir[len] = 0;
    ret = sys_chdir(dir);
    return ret;
}

int dup(int fd)
{
    return sys_dup(fd);
}

int dup2(int fd1, int fd2)
{
    return sys_dup2(fd1, fd2);
}

int getgid()
{
    return sys_getgid();
}

int setgid(int gid)
{
    return sys_setgid(gid);
}

int setuid(int uid)
{
    return sys_setuid(uid);
}

int ioctl(int fd, long request, void *arg)
{
    return sys_ioctl(fd, request, arg);
}

int stat(const char *path, struct stat *buf)
{
    return sys_stat(path, buf);
}

int creat(const char *path, int mode)
{
    return sys_creat(path, mode);
}

int execve(char *path, char *const argp[], char *const envp[])
{
    return sys_execve(path, argp, envp);
}

int execv(char *path, char **argp)
{
    return sys_execv(path, argp);
}

int fork()
{
    return sys_fork();
}

void thread_yield(void)
{
    sys_thread_yield();
}

void exit(int status)
{
    sys_exit(status);
}

int wait(int *stat_loc)
{
    return sys_wait(stat_loc);
}

int fstat(int fd, struct stat*buf)
{
    return sys_fstat(fd, buf);
}

long sleep(long ms)
{
    return sys_sleep(ms);
}

int pipe(int *p)
{
    return sys_pipe(p);
}

int thread_create(tid_t *tid, void *(*func)(void *), void *arg)
{
    return sys_thread_create(tid, func, arg);
}

int thread_self(void)
{
    return sys_thread_self();
}

int thread_join(tid_t __tid, void *__res)
{
    return sys_thread_join(__tid, __res);
}

void thread_exit(void *_exit)
{
    sys_thread_exit(_exit);
}

int thread_cancel()
{
    return sys_thread_cancel();
}

int park()
{
    return sys_park();
}

int unpark(tid_t tid)
{
    return sys_unpark(tid);
}

pid_t getpgrp(void)
{
    return sys_getpgrp();
}

pid_t getpgid(pid_t pid)
{
    return sys_getpgid(pid);
}

pid_t setpgrp(void)
{
    return sys_setpgrp();
}

int setpgid(pid_t pid, pid_t pgid)
{
    return sys_setpgid(pid, pgid);
}

pid_t getsid(pid_t pid)
{
    return sys_getsid(pid);
}

pid_t setsid(void)
{
    return sys_setsid();
}

int chown(const char *pathname, uid_t uid, gid_t gid)
{
    return sys_chown(pathname, uid, gid);
}

int fchown(int fd, uid_t uid, gid_t gid)
{
    return sys_fchown(fd, uid, gid);
}

void (*signal(int sig, void (*handler)(int)))(int)
{
    return (void (*)(int))sys_signal(sig, handler);
}

int kill(pid_t pid, int sig)
{
    return sys_kill(pid, sig);
}

int pause(void)
{
    return sys_pause();
}

int openpt(int flags)
{
    return sys_openpt(flags);
}

int grantpt(int fd)
{
    return sys_grantpt(fd);
}

int ptsname_r(int fd, char *buf, long buflen)
{
    return sys_ptsname_r(fd, buf, buflen);
}

#include <stddef.h>

char *ptsname(int fd)
{
    static char buf[256] = {0};
    return ptsname_r(fd, buf, sizeof buf) == 0 ? buf : NULL;
}

#include <bits/errno.h>

int isatty(int fd)
{
    int ret;
    ret = sys_isatty(fd);

    switch (ret)
    {
    case 1:
        break;
    default:
        ret = 0;
        break;
    }
    return ret;
}

int unlockpt(int fd)
{
    return sys_unlockpt(fd);
}

void *mmap(void *addr, size_t length, int prot, int flags,
           int fd, off_t offset)
{
    return sys_mmap(addr, length, prot, flags, fd, offset);
}

int munmap(void *addr, size_t length)
{
    return sys_munmap(addr, length);
}

int mprotect(void *addr, size_t len, int prot)
{
    return sys_mprotect(addr, len, prot);
}