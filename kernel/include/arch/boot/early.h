#pragma once

#include <lib/stddef.h>

int earlycons_putc(int c);
size_t earlycons_puts(const char *);
void earlycons_clr(void);
void earlycons_setcolor(int back, int fore);
int earlycons_fini(void);
void earlycons_restore_color(void);