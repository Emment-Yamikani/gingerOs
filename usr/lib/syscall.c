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

/* Signals */

#define SYS_KILL            29 //* send signal
#define SYS_SIGNAL          30 //* set signal handler
#define SYS_PAUSE           31 //* wait for a signal

/* Thread management */

#define SYS_THREAD_CREATE   32 //* create a thread
#define SYS_THREAD_SELF     33 //* get thread ID
#define SYS_THREAD_JOIN     34 //* wait for thread TID to terminate
#define SYS_THREAD_EXIT     35 //* terminate thread
#define SYS_THREAD_YIELD    36 //* create a thread

#define SYS_GETPGRP         37 //* get process group ID
#define SYS_SETPGRP         38 //* set process group ID
#define SYS_SETSID          39 //* set session ID
#define SYS_GETSID          40 //* get session ID
#define SYS_GETPGID         41 //* get process group ID of process PID
#define SYS_SETPGID         42 //* set process group ID of process PID

/* Terminals & Pseudo-terminals */

#define SYS_OPENPT          43 //* open pseudo-terminal
#define SYS_GRANTPT         44 //* grant pseudo-terminal permissions
#define SYS_PTSNAME         45 //* get pseudo-terminal name
#define SYS_PTSNAME_R       46 //* get pseudo-terminal name
#define SYS_ISATTY          47 //* isatty?

#define SYS_GETPAGESIZE     48 //* get kernel page size (currently not dynamic)
#define SYS_MMAN            49

#define SYS_BRK             50

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

#include <sys/stat.h>
#include <types.h>

extern int sys_getpid(void);
extern int sys_getppid(void);
extern int sys_kputc(int c);
extern int sys_open(const char *path, int mode, ...);
extern int sys_read(int fd, void *__buf, unsigned int size);
extern int sys_getuid();
extern int sys_brk(void *addr);
extern void *sys_sbrk(int nbytes);
extern int sys_getpagesize(void);
extern int sys_lseek(int fd, long off, int whence);
extern int sys_write(int fd, void *buf, unsigned int size);
extern int sys_close(int fd);
extern char *sys_getcwd(char *__buf, long __size);
extern int sys_chdir(char *dir);
extern int sys_dup(int fd);
extern int sys_dup2(int fd1, int fd2);
extern int sys_getgid();
extern int sys_setgid(int gid);
extern int sys_setuid(int uid);
extern int sys_ioctl(int fd, long request, void *arg);
extern int sys_stat(const char *path, struct stat *buf);
extern int sys_creat(const char *path, int mode);
extern int sys_execve(char *path, char *const argp[], char *const envp[]);
extern int sys_execv(char *path, char **argp);
extern int sys_fork();
extern void sys_thread_yield(void);
extern void sys_exit(int status);
extern int sys_wait(int *stat_loc);
extern int sys_fstat(int fd, struct stat*buf);
extern long sys_sleep(long ms);
extern int sys_pipe(int *p);
extern int sys_thread_create(tid_t *tid, void *(*func)(void *), void *arg);
extern int sys_thread_self(void);
extern int sys_thread_join(tid_t __tid, void *__res);
extern void sys_thread_exit(void *_exit);
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
extern char *sys_ptsname(int fd, char *buf, long buflen);
extern int sys_isatty(int fd);

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
    return sys_brk(addr);
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
    return lseek(fd, off, whence);
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

char *ptsname(int fd)
{
    static char buf[512];
    return sys_ptsname(fd, buf, sizeof buf);
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