#pragma once

#define NTTY 4
#define _SC_UP      0xE2
#define _SC_LEFT    0xE4
#define _SC_RIGHT   0xE5
#define _SC_DOWN    0xE3

extern volatile int use_gfx_cons;

int lfbterm_init(void);
void lfbterm_putc(int c);
int lfb_console_init(void);
void lfbterm_clrscrn(void);
void lfbterm_savecolor(void);
void lfbterm_restorecolor(void);
int lfbterm_puts(const char *s);
void lfbterm_setcolor(int bg, int fg);
void lfbterm_fill_rect(int x, int y, int w, int h, int color);
