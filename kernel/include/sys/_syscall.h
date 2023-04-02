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
#define SYS_WAITPID         55 //* wait for a specific process to change state
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
#define SYS_MPROTECT        29
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
#define SYS_THREAD_CANCEL   56

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
#define SYS_SETPARK         59


#include <lib/types.h>
#include <sys/_stat.h>
#include <bits/dirent.h>

extern int sys_kputc(void);

/*File Management*/

extern int sys_open(void);
extern int sys_read(void);
extern off_t sys_lseek(void);
extern int sys_write(void);
extern int sys_close(void);
extern char *sys_getcwd(void);
extern int sys_chdir(void);
extern int sys_readdir(void);
extern int sys_pipe(void);
extern int sys_dup(void);
extern int sys_dup2(void);
extern int sys_fstat(void);
extern int sys_stat(void);
extern int sys_ioctl(void);
extern int sys_creat(void);

/*Memory management*/

extern int sys_getpagesize(void);
extern int sys_mprotect(void);
extern void *sys_sbrk(void);
extern void *sys_mmap(void);
extern int sys_munmap(void);

/*Protection*/

extern uid_t sys_getuid(void);
extern gid_t sys_getgid(void);
extern int sys_setgid(void);
extern int sys_setuid(void);

int sys_chown(void);
int sys_fchown(void);

/*Process management*/

extern int sys_getpid(void);
extern long sys_sleep(void);
extern void sys_exit(void);

extern int sys_fork(void);
extern pid_t sys_wait(void);
extern pid_t sys_waitpid(void);
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
extern int sys_thread_cancel(void);
extern int sys_thread_join(void);
extern void sys_thread_exit(void);
extern int sys_park(void);
extern int sys_unpark(void);
extern void sys_setpark(void);

/*Signals*/

extern void (*sys_signal(void))(int);
extern int sys_kill(void);
extern int sys_pause(void);

/*Pseudo terminals*/

extern int sys_isatty(void);
extern int sys_grantpt(void);
extern int sys_openpt(void);
extern int sys_unlockpt(void);
extern int sys_ptsname_r(void);

#include <arch/context.h>
void syscall_stub(trapframe_t *);