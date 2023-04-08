CGA_BASE equ 0xB8000
VMA_HIGH equ 0xC0000000

CGA_BLACK           EQU 0
CGA_BLUE            EQU 1
CGA_GREEN           EQU 2
CGA_CYAN            EQU 3
CGA_RED             EQU 4
CGA_MAGENTA         EQU 5
CGA_BROWN           EQU 6
CGA_LIGHT_GREY      EQU 7
CGA_DARK_GREY       EQU 8
CGA_LIGHT_BLUE      EQU 9
CGA_LIGHT_GREEN     EQU 10
CGA_LIGHT_CYAN      EQU 11
CGA_LIGHT_RED       EQU 12
CGA_LIGHT_MAGENTA   EQU 13
CGA_YELLOW          EQU 14
CGA_WHITE           EQU 15

cga_pos dd 0
cga_attr dd CGA_CYAN


cga_putc_at:
    push edx
    mov eax, dword[esp + 0x8]
    mov edx, dword[esp + 0xC]
    shl edx, 16
    mov dx, 2
    mul dx
    shr edx, 16
    xchg edx, eax
    add edx, CGA_BASE
    mov ah, 0x0F
    mov word [edx], ax
    pop edx
    ret
;

cga_setattr:
    mov eax, [esp + 4]
    mov byte [cga_attr - VMA_HIGH], al
    ret
cga_getattr:
    xor eax, eax
    mov al, byte [cga_attr - VMA_HIGH]
    ret

cga_setcursor:
    push edx
    push ecx
    
    mov al, 14
    mov dx, 0x3d4
    out dx, al
    
    mov eax, dword [cga_pos - VMA_HIGH]
    xor edx, edx
    mov ecx, 2
    div ecx
    mov cx, ax
    
    xchg ah, al
    mov dx, 0x3d5
    out dx, al

    mov al, 15
    mov dx, 0x3d4
    out dx, al
    
    mov al, cl
    mov dx, 0x3d5
    out dx, al

    pop ecx
    pop edx
    ret
;
cga_scroll:
    push ecx
    push edi
    push esi

    mov edi, CGA_BASE
    mov esi, CGA_BASE + 160
    mov ecx, 0xF00
    rep movsb

    mov ecx, 160
    mov edi, CGA_BASE + (80 * 24 * 2)
    mov eax, dword[cga_attr - VMA_HIGH]
    xchg ah, al
    mov al, 0x20
.clr:
    mov word[edi], ax
    add edi, 2
    loop .clr

    mov dword[cga_pos], 80 * 24 * 2
    call cga_setcursor

    pop esi
    pop edi
    pop ecx
    ret




cga_putc:
    mov eax, [esp + 4]
    push edx
    push ecx
    
    cmp al, 0xA ;\n ?
    je .nline
    cmp al, 0x8 ; \b ?
    je .bspace
    cmp al, 0xD ; \r ?
    je .creturn
    cmp al, 0x9 ;\t ?
    je .done
    cmp al, 27; \e ?
    je .done

    cmp al, 0x20
    jge .put
    jl .done

.nline:
    mov ecx, 160
    add dword[cga_pos - VMA_HIGH], ecx
    xor edx, edx
    mov eax, dword[cga_pos - VMA_HIGH]
    div ecx
    sub dword[cga_pos - VMA_HIGH], edx
    jmp .done
;
.bspace:
    sub dword[cga_pos - VMA_HIGH], 2
    push ' '
    call cga_putc
    add esp, 4
    sub dword[cga_pos - VMA_HIGH], 2
    jmp .done
;
.creturn:
    mov ecx, 160
    mov eax, dword[cga_pos - VMA_HIGH]
    xor edx, edx
    div ecx
    mul ecx
    mov dword[cga_pos - VMA_HIGH], eax
    jmp .done
;
.put:
    mov edx, CGA_BASE
    add edx, dword[cga_pos - VMA_HIGH]
    add dword[cga_pos - VMA_HIGH], 2
    mov ah, byte [cga_attr - VMA_HIGH]
    mov word[edx], ax
.done:
    call cga_setcursor
    cmp dword[cga_pos], 80 * 24 * 2 + 160
    jl .return
    call cga_scroll
.return:
    pop ecx
    pop edx
    ret
;

cga_puts:
    push edx
    mov edx, dword[esp + 8]; string ptr
.begin:
    xor eax, eax
    mov al, byte[edx]
    test al, 0xFF
    jz .end

    push eax
    call cga_putc
    add esp, 4

    inc edx
    jmp .begin
.end:
    pop edx
    ret

;

print_dec:
    mov eax, [esp + 4]
    push edx
    push ecx
    push ebx
    push dword [cga_attr - VMA_HIGH]
    mov ecx, esp
    mov dword [cga_attr - VMA_HIGH], CGA_YELLOW
    mov ebx, 10
    push dword 0

.begin:
    xor edx, edx
    div ebx
    add edx, 0x30
    dec esp
    mov byte[esp], dl
    cmp eax, 0
    jnz .begin

    push esp
    call cga_puts
    mov esp, ecx

.done:
    pop dword [cga_attr - VMA_HIGH]
    pop ebx
    pop ecx
    pop edx
    ret
;

print_hex:
    mov eax, dword[esp + 4]
    push edx
    push ecx
    push dword[cga_attr - VMA_HIGH]
    mov dword [cga_attr - VMA_HIGH], CGA_LIGHT_BLUE
    mov ecx, esp
    push dword 0
.begin:
    mov edx, eax
    and edx, 0x0000000F
    cmp dl, 9
    jg .hex
    add dl, 0x30
    jmp .put
.hex:
    add dl, 0x37
.put:
    dec esp
    mov byte[esp], dl
    shr eax, 4
    cmp eax, 0
    jnz .begin

    push esp
    call cga_puts
    
.done:
    mov esp, ecx
    pop dword [cga_attr - VMA_HIGH]
    pop ecx
    pop edx
    ret
;

print_oct:
    mov eax, [esp + 4]
    push edx
    push ecx
    push ebx
    push dword[cga_attr - VMA_HIGH]
    mov dword [cga_attr - VMA_HIGH], CGA_LIGHT_MAGENTA
    mov ecx, esp
    push dword 8
    push dword 0
    mov ebx, esp
    add esp, 4
    mov dword[ebx], 0
.begin:
    dec ebx
    xor edx, edx
    div dword [esp]
    add edx, 0x30
    mov byte[ebx], 0
    mov byte[ebx], dl
    cmp eax, 0
    jnz .begin

    mov esp, ebx
    push ebx
    call cga_puts
    mov esp, ecx

.done:
    pop dword [cga_attr - VMA_HIGH]
    pop ebx
    pop ecx
    pop edx
    ret
;

global cga_printf
cga_printf:
    mov eax, esp
    push edi
    push ebx
    mov edi, dword[eax + 0x4] ;(string format)
    add eax, 8;(variables)
    mov ebx, eax

.fmt:
    test byte[edi], 0xFF
    jz .done

    cmp byte[edi], '%'
    je .format
.put:
    xor eax, eax
    mov al, byte[edi]
    push eax
    call cga_putc
    add esp, 4
    jmp .next
;
.format:
    inc edi
    cmp byte[edi], '%'
    je .put
    cmp byte[edi], 'c'
    je .putc
    cmp byte[edi], 's'
    je .puts
    cmp byte[edi], 'x'
    je .putx
    cmp byte[edi], 'X'
    je .putx
    cmp byte[edi], 'p'
    je .putx
    cmp byte[edi], 'd'
    je .putd
    cmp byte[edi], 'D'
    je .putd
    cmp byte[edi], 'i'
    je .putd
    cmp byte[edi], 'o'
    je .puto
    jmp .put
;
.putc:
    xor eax, eax
    mov al, byte[ebx]
    add ebx, 4
    
    push eax
    call cga_putc
    add esp, 4
    jmp .next
;
.puts:
    xor eax, eax
    mov eax, dword[ebx]
    add ebx, 4
    
    push dword[cga_attr - VMA_HIGH]
    mov dword [cga_attr - VMA_HIGH], CGA_LIGHT_GREEN

    push eax
    call cga_puts
    add esp, 4

    pop dword [cga_attr - VMA_HIGH]
    jmp .next
;
.puto:
    xor eax, eax
    mov eax, dword[ebx]
    add ebx, 4
    
    push eax
    call print_oct
    add esp, 4
    jmp .next
;
.putd:
    xor eax, eax
    mov eax, dword[ebx]
    add ebx, 4
    
    push eax
    call print_dec
    add esp, 4
    jmp .next
;
.putx:
    xor eax, eax
    mov eax, dword[ebx]
    add ebx, 4

    push eax
    call print_hex
    add esp, 4

.next:
    inc edi
    jmp .fmt

.done:
    pop ebx
    pop edi
    ret

global cga_panic
cga_panic:
    push CGA_RED
    call cga_setattr
    add esp, 4

    push PANIC_MSG - VMA_HIGH
    call cga_printf
    add esp, 4
    mov eax, dword[esp + 4]
    push dword[esp + 8]
    push eax
    call cga_printf
    add esp, 4
    jmp $

PANIC_MSG db "[PANIC] ", 0