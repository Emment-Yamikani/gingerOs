align 4

%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0
    push dword %1
    jmp common_stub
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
    push dword %1
    jmp common_stub
%endmacro

%macro IRQ 2
global irq%1
irq%1:
    push dword 0
    push dword %2
    jmp common_stub
%endmacro

%macro pushsegs 0
    push ds
    push es
    push fs
    push gs
%endmacro

%macro popsegs 0
    pop gs
    pop fs
    pop es
    pop ds
%endmacro

SEG_KDATA   equ 2
SEG_KCPU    equ 6

%macro pushcontext 0
    pushsegs
    pushad

    ; setup data segments
    mov ax, (SEG_KDATA << 3)
    mov ds, ax
    mov es, ax
    mov fs, ax

    ; setup per-cpu segment
    mov ax, (SEG_KCPU << 3)
    mov gs, ax
%endmacro

%macro popcontext 0
    popad
    popsegs
%endmacro

ISR_NOERR 0
    ISR_NOERR 1
    ISR_NOERR 2
    ISR_NOERR 3
    ISR_NOERR 4
    ISR_NOERR 5
    ISR_NOERR 6
    ISR_NOERR 7
    ISR_ERR   8
    ISR_NOERR 9
    ISR_ERR   10
    ISR_ERR   11
    ISR_ERR   12
    ISR_ERR   13
    ISR_ERR   14
    ISR_NOERR 15
    ISR_NOERR 16
    ISR_NOERR 17
    ISR_NOERR 18
    ISR_NOERR 19
    ISR_NOERR 20
    ISR_NOERR 21
    ISR_NOERR 22
    ISR_NOERR 23
    ISR_NOERR 24
    ISR_NOERR 25
    ISR_NOERR 26
    ISR_NOERR 27
    ISR_NOERR 28
    ISR_NOERR 29
    ISR_NOERR 30
    ISR_NOERR 31
    ISR_NOERR 128

IRQ   0,    32
    IRQ   1,    33
    IRQ   2,    34
    IRQ   3,    35
    IRQ   4,    36
    IRQ   5,    37
    IRQ   6,    38
    IRQ   7,    39
    IRQ   8,    40
    IRQ   9,    41
    IRQ  10,    42
    IRQ  11,    43
    IRQ  12,    44
    IRQ  13,    45
    IRQ  14,    46
    IRQ  15,    47
    IRQ  31,    63
IRQ  40,    72

extern trap

extern printk
global trapret
global forkret

common_stub:
    pushcontext
    push    dword esp
    call    trap
    add esp, 4
trapret:
    popcontext
    add     esp, 8 ;pop-off err and intno
    iret

forkret:
    popcontext
    add esp, 8
    mov eax, esp
    push eax
    push dword fmt
    call printk
    add esp, 8
    iret

fmt db "trapret: eip: %p, cs: %p, eflags: %p, esp: %p, ss: %p", 0xa, 0xd, 0
kfmt db "trapret: eip: %p, cs: %p, eflags: %p", 0xa, 0