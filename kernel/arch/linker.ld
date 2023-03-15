ENTRY(_start)

SECTIONS
{
	. = 0xC0100000;
	_kernel_start = .;
    .text ALIGN (4K) : AT (ADDR (.text) - 0xC0000000)
    {
        *(.multiboot)
        *(.text)
    }

    .__minit ALIGN (4K) : AT(ADDR(.__minit) - 0xC0000000)
    {
        __minit = .;
        *(.__minit*)
        __minit_end = .;
    }

    .__einit ALIGN (4K) : AT(ADDR(.__einit) - 0xC0000000)
    {
        __einit = .;
        *(.__einit*)
        __einit_end = .;
    }

    .__efini ALIGN (4K) : AT(ADDR(.__efini) - 0xC0000000)
    {
        __efini = .;
        *(.__efini*)
        __efini_end = .;
    }

    .__builtin_thread_entry ALIGN (4K) : AT(ADDR(.__builtin_thread_entry) - 0xC0000000)
    {
        __builtin_thread_entry = .;
        *(.__builtin_thread_entry*)
        __builtin_thread_entry_end = .;
    }

    .__builtin_thread_arg ALIGN (4K) : AT(ADDR(.__builtin_thread_arg) - 0xC0000000)
    {
        __builtin_thread_arg = .;
        *(.__builtin_thread_arg*)
        __builtin_thread_arg_end = .;
    }

    .rodata ALIGN (4K) : AT (ADDR (.rodata) - 0xC0000000)
    {
        *(.rodata)
    }
    .data ALIGN (4K) : AT (ADDR (.data) - 0xC0000000)
    {
        *(.data)
    }
    .bss ALIGN (4K) : AT (ADDR (.bss) - 0xC0000000)
    {
        *(.bss)
    }
    _kernel_end = .;
}