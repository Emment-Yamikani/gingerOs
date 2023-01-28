extern init
extern main
global _start
extern sys_exit
extern printf

_start:
    call main
    ret