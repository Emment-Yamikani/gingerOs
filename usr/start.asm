extern init
extern main
global _start
extern sys_exit

_start:
    call main
    push eax
    call sys_exit
    jmp $
    ret