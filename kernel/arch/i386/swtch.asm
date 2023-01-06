global swtch
swtch:
    cli
    mov edx, dword [esp + 4]
    mov eax, dword [esp + 8]

    push dword edi
    push dword esi
    push dword ebp
    push dword ebx

    mov dword [edx], esp
    mov esp, eax

    pop dword ebx
    pop dword ebp
    pop dword esi
    pop dword edi
    ret