bits 32
MULTIBOOT_ALIGN equ 1<<0             ;align loaded modules on page boundaries
MEMINFO         equ 1<<1             ;provide memory map
VIDEOMODE       equ 1<<2             ;set video mode
MAGIC           equ 0x1BADB002       ;'magic number' lets bootloader find the header
FLAGS           equ MULTIBOOT_ALIGN | MEMINFO | VIDEOMODE  ;this is the Multiboot 'flag' field
CHECKSUM        equ -(MAGIC + FLAGS) ;checksum of above, to prove we are multiboot

section .multiboot
align 4
mboot:
    dd  MAGIC
    dd  FLAGS
    dd  CHECKSUM    ;/* checksum */
    dd 0            ;/* header_addr */
    dd 0            ;/* heload_addr */
    dd 0            ;/* heload_end_addr */
    dd 0            ;/* hebss_end_addr */
    dd 0            ;/* heentry_addr */
;define video setting
    dd 0            ;/* mode_type */
    dd 0            ;/* mowidth */
    dd 0            ;/* moheight */
    dd 32           ;/* modepth */

section .text
align 4

extern _kernel_end
extern early_init
extern early_exit
extern __early_init
extern __early_fini
extern process_multiboot_info ;(multiboot_info_t *info)
extern panic
extern tvinit
extern cpu_init

global _pgdir

PAGESZ        equ 0x1000      ; page size
VMA_HIGH    equ 0xC0000000  ; kernel base address
STACKSZ     equ 0x8000      ; 4kib bootstrap stack
MULTIBOOT_BOOTLOADER_MAGIC equ 0x2BADB002
LOWER_HALF  equ 0x1000000   ; 1Mib, static memory(paged in from start)

global _start

_start:
    cli
    mov esp, stack_top - VMA_HIGH
    push dword ebp
    mov ebp, esp    ;create new kernel stack frame
    
    push dword ebx
    push dword eax

    push dword mmdev_pt_end - _pgdir
    push dword (_pgdir - VMA_HIGH)
    call clr
    add esp, 8

    call identity_map

    mov eax, _pgdir - VMA_HIGH ; set page directory
    mov edx, eax
    mov cr3, edx
    or eax, 0x1B
    mov dword [edx + 0xffc], eax

    mov eax, pagetable0 - VMA_HIGH ; map in pagetable 0
    or eax, 0x1B
    mov ebx, 0
    mov ecx, 0xC00

    .map_tables:
        mov dword[edx + ebx], eax
        mov dword[edx + ecx], eax
        add ebx, 4
        add ecx, 4
        add eax, PAGESZ
        cmp eax, pt_end - VMA_HIGH
        jl .map_tables
    
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax
    xor eax, eax
    mov cr2, eax    ; clear fault address register

    lea eax, [.higher_half]
    jmp eax

    .higher_half:
        mov eax, cr3
        mov cr3, eax

        add esp, VMA_HIGH

        lea eax, [tvinit]
        call eax

        lea eax, [cpu_init]
        call eax
        
        lea eax, [__early_init]
        call eax

        lea eax, [process_multiboot_info]
        call eax

        cmp eax, 0
        jne process_multiboot_info_error

        lea eax, [__early_fini]
        call eax

        lea eax, [early_init]
        call eax

    jmp $
;
identity_map:
    mov edx, pagetable0 - VMA_HIGH
    xor eax, eax
    or eax, 0x1B
    .map:
        mov dword[edx], eax
        add edx, 4
        add eax, PAGESZ
        cmp eax, LOWER_HALF
        jl .map

    call map_mmdevs
    retd
;

MMDEV_ADDR  equ 0xFE000000

map_mmdevs:
    mov edx, (_pgdir - VMA_HIGH) + 0xFE0
    mov eax, mmdev_pt - VMA_HIGH
    or eax, 0x1B
    .map_pt:
        mov dword [edx], eax
        add edx, 4
        add eax, PAGESZ
        cmp eax, mmdev_pt_end - VMA_HIGH
        jl .map_pt
    ;
    mov edx, mmdev_pt - VMA_HIGH
    mov eax, MMDEV_ADDR
    or eax, 0x1B
    .map:
        mov dword[edx], eax
        add edx, 4
        add eax, PAGESZ
        cmp eax, 0xFFC00000
        jl .map
    retd

clr:
    mov ecx, dword [esp + 8]
    mov edx, dword [esp + 4]
    .clr:
        mov byte [edx], 0
        inc edx
        loop .clr
    retd

err_bootldr db "process_multiboot_info_error", 0

process_multiboot_info_error:
    push err_bootldr
    call panic
    jmp $

section .data
global pgtable_left
pgtable_left: dd 0

section .bss
align 16
resb STACKSZ
stack_top:

align PAGESZ
_pgdir:
resb PAGESZ

align PAGESZ
pagetable0:
resb PAGESZ * 4
global pt_end
pt_end:

align PAGESZ
mmdev_pt:
resb PAGESZ * 7
global mmdev_pt_end
mmdev_pt_end: