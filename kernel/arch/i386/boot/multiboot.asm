extern cga_printf
extern cga_panic

global get_mboot_info
get_mboot_info:





section .bss
align 16
multiboot_info:
    dd 0