#include "../include/stdio.h"
#include "../include/stdlib.h"

extern void exit(int);

__attribute__((__noreturn__))
void abort(void) {
#if defined(__is_libk)
	// TODO: Add proper kernel panic.
	printf("kernel: panic: abort()\n");
#else
	// TODO: Abnormally terminate the process as if by SIGABRT.
	printf("abort()\n");
	exit(1);
#endif
	while (1) { }
	__builtin_unreachable();
}
