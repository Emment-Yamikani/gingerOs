extern main
extern init
global _start

_start:
    call main
    jmp $
    ret