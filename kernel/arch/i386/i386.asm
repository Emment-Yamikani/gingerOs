global _xchg_
global _kstart
global _ustart
global read_tsc
global read_esp
global read_ebp
global read_eax
global read_cr0
global write_cr0
global read_cr2
global read_cr3
global write_cr3
global gdt_flush
global idt_flush
global tss_flush
global read_eflags
global paging_invlpg
global disable_caching

read_eax:
    ret

read_tsc:
    rdtsc
    ret

_kstart:
    cli
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov ss, ax
    sti
    ret

disable_caching:
    mov eax, cr0
    and eax, 0x8000000f
    mov cr0, eax
    ret

paging_invlpg:
    mov eax, dword [esp + 4]
    invlpg [eax]
    ret

write_cr3:
    mov eax, dword [esp + 4]
    mov cr3, eax
    ret

read_cr3:
    mov eax, cr3
    ret
    
read_cr2:
    mov eax, cr2
    ret

read_cr0:
    mov eax, cr0
    ret

write_cr0:
    mov eax, dword[esp + 4]
    mov cr0, eax
    ret

read_eflags:
    pushfd
    pop dword eax
    ret

read_esp:
    mov eax, esp
    ret

read_ebp:
    mov eax, ebp
    ret

_xchg_:
    push ebx
    ;0: ebx, 4:ret, 8:ptr, 12:newval
    mov ebx, dword [esp + 8]
    mov eax, dword [esp + 12]
    lock xchg dword [ebx], eax
    pop ebx
    ret

gdt_flush:
    mov eax, dword [esp+4]
    lgdt [eax]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush
.flush:
    ret

idt_flush:
    mov eax, dword [esp + 4]
    lidt [eax]
    ret

tss_flush:
    mov eax, dword [esp + 4]
    ltr ax
    ret