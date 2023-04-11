extern cga_panic
extern cga_printf
extern _kernel_end

VMA_HIGH equ 0xC0000000

section .text
global __alignup
__alignup:
    mov eax, [esp + 4]
    test eax, 0xfff
    jz .done
    add eax, 0x1000
    and eax, ~0xfff
    .done:
        ret
;

global __aligndown
__aligndown:
    mov eax, [esp + 4]
    and eax, ~0xfff
    ret
;

global __get_boot_info
__get_boot_info:
    cmp dword [esp + 4], 0x2BADB002
    jne .not_multiboot

    push (_kernel_end - VMA_HIGH)
    call __alignup
    add esp, 4

    mov dword [__last_addr - VMA_HIGH], eax

    mov eax, esp
    pusha
    mov edx, dword [eax + 8] ; multiboot_info_t *.
    mov dword [mboot - VMA_HIGH], edx

    mov eax, dword [edx + 0]
    mov dword [__flags - VMA_HIGH], eax

    .process_mmaps_info:
        test dword [edx + 0], (1 << 6)
        jz .process_mods_info

        mov ecx, dword [edx + 44]; mmap_length
        mov edi, dword [edx + 48]; mmap_addr
        add ecx, edi
        push ecx ; save buffer limit

        mov ecx, (__mmaps - VMA_HIGH)
        mov esi, edi ;mmap*
        ; 0     mmap->size
        ; 4     mmap->addr
        ; 12    mmap->len
        ; 20    mmap->type
        .loop:
            ;mmap->addr
            mov eax, dword [edi + 4]
            mov dword [ecx + 0], eax
            mov eax, dword [edi + 8]
            mov dword [ecx + 4], eax

            ;mmap->len
            mov ebx, edx
            xor edx, edx
            mov eax, dword [edi + 12]
            mov dword [ecx + 8], eax

            push 1024
            div dword [esp]

            add dword[__mem_size - VMA_HIGH], eax
            
            mov eax, dword [edi + 16]
            mov dword [ecx + 12], eax

            xor edx, edx
            div dword [esp]
            add dword[__mem_size - VMA_HIGH], eax

            mov edx, ebx
            add esp, 4

            ;mmap->type
            mov eax, dword [edi + 20]
            mov dword [ecx + 16], eax

            add edi, dword [esi + 0] ; mmap->size
            add edi, 4
            mov esi, edi
            add ecx, 20
            inc dword [__mmap_count - VMA_HIGH]
            cmp edi, dword [esp + 0] ;
            jl .loop
        ;
        add esp, 4

    .process_mods_info:
        test dword [edx + 0], (1 << 3)
        jz .done
        
        mov ecx, dword [edx + 20] ;mods_count.
        cmp ecx, 0
        jz .done

        mov esi, dword [edx + 24] ; mods_addr.
        mov edi, __mods - VMA_HIGH
        .loop1:
            mov eax, dword [esi + 0]
            mov dword [edi + 0], eax
            
            mov eax, dword [esi + 4]
            sub eax, dword [esi + 0]
            mov dword [edi + 4], eax

            mov eax, dword [esi + 8]
            mov dword [edi + 8], eax

            push dword [esi + 4]
            call __alignup
            add esp, 4

            cmp eax, [__last_addr - VMA_HIGH]
            jl .next_mod
            mov dword [__last_addr - VMA_HIGH], eax

            .next_mod:
                add edi, 12
                add esi, 12
                inc dword [__mods_count - VMA_HIGH]
            loop .loop1
    .done:
        popa
        ret
    .not_multiboot:
        push panic_msg - VMA_HIGH
        call cga_panic
;
global __alloc_frame;
__alloc_frame:
    push edx
    push ecx
    mov eax, dword [(__last_addr - VMA_HIGH) + 0]
    add dword [(__last_addr - VMA_HIGH) + 0], 0x1000
    mov edx, eax
    mov ecx, 0x1000
    .zero:
        mov byte [edx], 0
        inc edx
        loop .zero
    pop ecx
    pop edx
    ret
;

global __get_mem_size
__get_mem_size:
    mov eax, dword [__mem_size - VMA_HIGH]
    ret

section .bss
align 16
global boot_info
boot_info:
    __flags:        resb 4
    __last_addr:    resb 8
    __mem_size:     resb 8
    __mmaps:        resb 32 * 20
    __mmap_count:   resb 4
    __mods:         resb 32 * 12
    __mods_count:   resb 4
__boot_info_end:
;

section .data
global mboot
mboot dd 0
size_str db "size: %d", 0xa, 0
MOD_SIZE_MSG db "Size Of Module %pB", 0
ADDR db "ADDR: 0x%p", 0xa, 0
panic_no_modules db "No modules", 0
panic_msg db "error getting info", 0
count_str db "count: %d", 0xa, 0