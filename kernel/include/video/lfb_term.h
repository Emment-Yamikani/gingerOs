#pragma once

extern volatile int use_gfx_cons;

int lfb_term_init(void);
void lfb_term_putc(int c);
int lfb_term_puts(const char *s);