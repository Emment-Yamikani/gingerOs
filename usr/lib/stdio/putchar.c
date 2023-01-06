#include "../include/stdio.h"

#if defined(__is_libk)
//#include "../../kernel/include/kernel/tty.h"
#endif

extern long write(int, char *, long);

int putchar(int ic) {
#if defined(__is_libk)
	char c = (char) ic;
	terminal_write(&c, sizeof(c));
#else
	// TODO: Implement stdio and the write system call.
	write(STDOUT_FILENO, (char *)&ic, 1);
#endif
	return ic;
}
