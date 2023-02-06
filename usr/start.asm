extern init
extern main
global _start
extern sys_exit
extern printf

_start:
    mov eax, esp
    push dword [eax + 12]
    push dword [eax + 8]
    push dword [eax + 4]
    call main
    add esp, 12
    ret