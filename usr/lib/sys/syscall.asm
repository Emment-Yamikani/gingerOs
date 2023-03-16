;-------------------------------------------------------------------
;---------------------------Miscelleneous.--------------------------
;-------------------------------------------------------------------

%define SYS_KPUTC           0 ;put a character to the kernel's terminal

;-------------------------------------------------------------------
;---------------------------Process management----------------------
;-------------------------------------------------------------------

%define SYS_EXECVE          1 ;execve
%define SYS_FORK            2 ;fork a child
%define SYS_YIELD           3 ;yield cpu
%define SYS_EXIT            4 ;exit the process
%define SYS_WAIT            5 ;wait for child to terminate
%define SYS_SLEEP           6 ;sleep for n secs.
%define SYS_GETPID          7 ;get process ID
%define SYS_EXECV           8 ;exec
%define SYS_GETPPID         9 ;get process' parent ID

;-------------------------------------------------------------------
;---------------------------Protection------------------------------
;-------------------------------------------------------------------

%define SYS_SETGID          10 ;set real group ID
%define SYS_SETUID          11 ;set real user ID
%define SYS_GETGID          12 ;get real group ID
%define SYS_GETUID          13 ;set real user ID

;-------------------------------------------------------------------
;---------------------------Standard IO-----------------------------
;-------------------------------------------------------------------

%define SYS_CREAT	        14 ;create a file
%define SYS_STAT	        15 ;get file stats.
%define SYS_FSTAT           16 ;get file stats.
%define SYS_LSEEK           17 ;seek in file
%define SYS_WRITE	        18 ;write to file
%define SYS_CLOSE	        19 ;close file
%define SYS_PIPE            20 ;open an unamed pipe
%define SYS_OPEN	        21 ;open file
%define SYS_READ	        22 ;read from file
%define SYS_CHDIR           23 ;change working directory
%define SYS_GETCWD          24 ;get current working directory
%define SYS_DUP             25 ;duplicate fildes
%define SYS_DUP2            26 ;duplicate fildes
%define SYS_IOCTL           27 ;ioctl

;-------------------------------------------------------------------
;---------------------------Memory management-----------------------
;-------------------------------------------------------------------

%define SYS_SBRK	        28 ;sbrk
%define SYS_MPROTECT        29
%define SYS_GETPAGESIZE     30 ;get kernel page size (currently not dynamic)
%define SYS_MMAP            31

;-------------------------------------------------------------------
;---------------------------Signals---------------------------------
;-------------------------------------------------------------------

%define SYS_KILL            32 ;send signal
%define SYS_SIGNAL          33 ;set signal handler
%define SYS_PAUSE           34 ;wait for a signal

;-------------------------------------------------------------------
;---------------------------Thread management-----------------------
;-------------------------------------------------------------------

%define SYS_THREAD_CREATE   35 ;create a thread
%define SYS_THREAD_SELF     36 ;get thread ID
%define SYS_THREAD_JOIN     37 ;wait for thread TID to terminate
%define SYS_THREAD_EXIT     38 ;terminate thread
%define SYS_THREAD_YIELD    39 ;create a thread

%define SYS_GETPGRP         40 ;get process group ID
%define SYS_SETPGRP         41 ;set process group ID
%define SYS_SETSID          42 ;set session ID
%define SYS_GETSID          43 ;get session ID
%define SYS_GETPGID         44 ;get process group ID of process PID
%define SYS_SETPGID         45 ;set process group ID of process PID

;--------------------------------------------------------------------
;--------------Terminals & Pseudo-terminals--------------------------
;--------------------------------------------------------------------

%define SYS_OPENPT          46 ;open pseudo-terminal
%define SYS_GRANTPT         47 ;grant pseudo-terminal permissions
%define SYS_PTSNAME_R       48 ;get pseudo-terminal name
%define SYS_ISATTY          49 ;isatty?
%define SYS_UNLOCKPT        50 ;unlock a pty pair

;/*System configurations*/

%define SYS_MUNMAP          51

;/*Directory manipulation*/

%define SYS_READDIR         52
%define SYS_CHOWN           53
%define SYS_FCHOWN          54

%macro STUB 2
global sys_%2
sys_%2:
    mov eax, %1
    int 0x80
    ret
%endmacro


STUB SYS_KPUTC, kputc
STUB SYS_EXECVE, execve
STUB SYS_FORK, fork
STUB SYS_YIELD, yield
STUB SYS_EXIT, exit
STUB SYS_WAIT, wait
STUB SYS_SLEEP, sleep
STUB SYS_GETPID, getpid
STUB SYS_EXECV, execv
STUB SYS_GETPPID, getppid
STUB SYS_SETGID, setgid
STUB SYS_SETUID, setuid
STUB SYS_GETGID, getgid
STUB SYS_GETUID, getuid
STUB SYS_CREAT, creat
STUB SYS_STAT, stat
STUB SYS_FSTAT, fstat
STUB SYS_LSEEK, lseek
STUB SYS_WRITE, write
STUB SYS_CLOSE, close
STUB SYS_PIPE, pipe
STUB SYS_OPEN, open
STUB SYS_READ, read
STUB SYS_CHDIR, chdir
STUB SYS_GETCWD, getcwd
STUB SYS_DUP, dup
STUB SYS_DUP2, dup2
STUB SYS_IOCTL, ioctl

STUB SYS_SBRK, sbrk
STUB SYS_GETPAGESIZE, getpagesize
STUB SYS_MMAP, mmap
STUB SYS_MUNMAP, munmap
STUB SYS_MPROTECT, mprotect

STUB SYS_KILL, kill
STUB SYS_SIGNAL, signal
STUB SYS_PAUSE, pause

STUB SYS_THREAD_CREATE, thread_create
STUB SYS_THREAD_SELF, thread_self
STUB SYS_THREAD_JOIN, thread_join
STUB SYS_THREAD_EXIT, thread_exit
STUB SYS_THREAD_YIELD, thread_yield

STUB SYS_GETPGRP, getpgrp
STUB SYS_SETPGRP, setpgrp
STUB SYS_SETSID, setsid
STUB SYS_GETSID, getsid
STUB SYS_GETPGID, getpgid
STUB SYS_SETPGID, setpgid

STUB SYS_OPENPT, openpt
STUB SYS_GRANTPT, grantpt
STUB SYS_PTSNAME_R, ptsname_r
STUB SYS_ISATTY, isatty
STUB SYS_UNLOCKPT, unlockpt

STUB SYS_READDIR, readdir

STUB SYS_CHOWN, chown
STUB SYS_FCHOWN, fchown