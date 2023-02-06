ENTRY(main)

SECTIONS
{
	. = 0x1000000;
	_prog_start = .;
    .text ALIGN (4K) :
    {
        height = .;
        *(.text)
    }
    .rodata ALIGN (4K) :
    {
        *(.rodata)
    }
    .data ALIGN (4K) :
    {
        *(.data)
    }
    .bss ALIGN (4K) :
    {
        *(.bss)
    }
    _prog_end = .;
}