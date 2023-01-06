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

;-------------------------------------------------------------------
;---------------------------Signals---------------------------------
;-------------------------------------------------------------------

%define SYS_KILL            29 ;send signal
%define SYS_SIGNAL          30 ;set signal handler
%define SYS_PAUSE           31 ;wait for a signal

;-------------------------------------------------------------------
;---------------------------Thread management-----------------------
;-------------------------------------------------------------------

%define SYS_THREAD_CREATE   32 ;create a thread
%define SYS_THREAD_SELF     33 ;get thread ID
%define SYS_THREAD_JOIN     34 ;wait for thread TID to terminate
%define SYS_THREAD_EXIT     35 ;terminate thread
%define SYS_THREAD_YIELD    36 ;create a thread

%define SYS_GETPGRP         37 ;get process group ID
%define SYS_SETPGRP         38 ;set process group ID
%define SYS_SETSID          39 ;set session ID
%define SYS_GETSID          40 ;get session ID
%define SYS_GETPGID         41 ;get process group ID of process PID
%define SYS_SETPGID         42 ;set process group ID of process PID

;--------------------------------------------------------------------
;--------------Terminals & Pseudo-terminals--------------------------
;--------------------------------------------------------------------

%define SYS_OPENPT          43 ;open pseudo-terminal
%define SYS_GRANTPT         44 ;grant pseudo-terminal permissions
%define SYS_PTSNAME         45 ;get pseudo-terminal name
%define SYS_PTSNAME_R       46 ;get pseudo-terminal name
%define SYS_ISATTY          47 ;isatty?

%define SYS_GETPAGESIZE     48 ;get kernel page size (currently not dynamic)
%define SYS_MMAN            49

%define SYS_BRK             50


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
STUB SYS_PTSNAME, ptsname
STUB SYS_PTSNAME_R, ptsname_r
STUB SYS_ISATTY, isatty
STUB SYS_GETPAGESIZE, getpagesize
STUB SYS_MMAN, mman
STUB SYS_BRK, brk