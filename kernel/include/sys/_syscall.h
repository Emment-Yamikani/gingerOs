#pragma once

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

#include <lib/types.h>
#include <sys/_stat.h>

extern int sys_kputc(void);

/*File Management*/

extern int sys_open(void);
extern int sys_read(void);
extern off_t sys_lseek(void);
extern int sys_write(void);
extern int sys_close(void);
extern char *sys_getcwd(void);
extern int sys_chdir(void);
extern int sys_pipe(void);
extern int sys_dup(void);
extern int sys_dup2(void);
extern int sys_fstat(void);
extern int sys_stat(void);
extern int sys_ioctl(void);
extern int sys_creat(void);

/*Memory management*/

extern int sys_getpagesize(void);
extern int sys_brk(void);
extern void *sys_sbrk(void);

/*Protection*/

extern uid_t sys_getuid(void);
extern gid_t sys_getgid(void);
extern int sys_setgid(void);
extern int sys_setuid(void);

/*Process management*/

extern int sys_getpid(void);
extern long sys_sleep(void);
extern void sys_exit(void);

extern int sys_fork(void);
extern int sys_wait(void);
extern int sys_execv(void);
extern int sys_execve(void);

/*Process relationships*/

extern int sys_getppid(void);
extern pid_t sys_getpgrp(void);
extern pid_t sys_setsid(void);
extern pid_t sys_setpgrp(void);
extern pid_t sys_getsid(void);
extern pid_t sys_getpgid(void);
extern int sys_setpgid(void);

/*Thread management*/

extern void sys_thread_yield(void);
extern int sys_thread_create(void);
extern int sys_thread_self(void);
extern int sys_thread_join(void);
extern void sys_thread_exit(void);

/*Signals*/

extern void (*sys_signal(int sig, void (*handler)(int)))(int);
extern int sys_kill(pid_t pid, int sig);
extern int sys_pause(void);

/*Pseudo terminals*/

extern int sys_isatty(int fd);
extern int sys_grantpt(int fd);
extern int sys_openpt(int flags);
extern int sys_ptsname_r(int fd, char *buf, long buflen);
extern char *sys_ptsname(int fd, char *buf, long buflen);

#include <arch/context.h>
void syscall_stub(trapframe_t *);