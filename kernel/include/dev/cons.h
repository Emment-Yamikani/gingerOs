#ifndef DEV_CONS_H
#define DEV_CONS_H

#include <lib/stddef.h>

#define CGA_BLACK           0
#define CGA_DARK_GREY       8
#define CGA_BLUE            1
#define CGA_LIGHT_BLUE      9
#define CGA_GREEN           2
#define CGA_LIGHT_GREEN     10
#define CGA_CYAN            3
#define CGA_LIGHT_CYAN      11
#define CGA_RED             4
#define CGA_LIGHT_RED       12
#define CGA_MAGENTA         5
#define CGA_LIGHT_MAGENTA   13
#define CGA_BROWN           6
#define CGA_YELLOW          14
#define CGA_LIGHT_GREY      7
#define CGA_WHITE           15


int cons_dev_init(void);

size_t cons_write(const char *buf, size_t n);
size_t cons_puts(const char *s);
void cons_clr();
void cons_putc(int c);
void cons_init();
void cons_save(void);
void cons_restore(void);
void cons_setcolor(int back, int fore);

#endif ///DEV_CONS_H