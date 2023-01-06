#include <sys/system.h>
#include <lib/stddef.h>
#include <dev/cons.h>
#include <lib/stddef.h>
#include <lib/stdint.h>
#include <arch/system.h>
#include <lib/string.h>


volatile int use_early_cons = 1;

#ifndef BACKSPACE
#define BACKSPACE 0x100
#endif // BACKSPACE

#define CGA_PTR ((uint16_t *)(0xC00B8000))
#define CGA_CTRL 0x3d4
#define CGA_DATA 0x3d5

// position in cga buffer
static int pos = 0;
static uint16_t cga_attrib = 0;
static uint16_t cga_save =0;

static void cga_setcursor(int pos)
{
    outb(CGA_CTRL, 14);
    outb(CGA_DATA, (uint8_t)(pos >> 8));
    outb(CGA_CTRL, 15);
    outb(CGA_DATA, (uint8_t)(pos));
}

static void earlycons_scroll()
{
    memmove(CGA_PTR, &CGA_PTR[80], 2 * ((80 * 25) - 80));
    pos -= 80;
    memsetw(&CGA_PTR[pos], ((cga_attrib << 8) & 0xff00) | (' ' & 0xff), 80);
}

static void earlycons_setpos(int _pos)
{
    if (_pos < 0)
        _pos = 0;
    else if (_pos > (80 * 25))
        _pos = 80 * 25;
    pos = _pos;
}

int earlycons_putc(int c)
{
    //uart_putc((char)c);

    if (c == BACKSPACE)
        c = '\b';
    if (c == '\n')
        pos += 80 - pos % 80;
    else if (c == '\b')
        CGA_PTR[--pos] = ((cga_attrib << 8) & 0xff00) | (' ' & 0xff);
    else if (c == '\t')
        pos = (pos + 4) & ~3;
    else if (c == '\r')
        pos = (pos / 80) * 80;
    if (c >= ' ')
        CGA_PTR[pos++] = ((cga_attrib << 8) & 0xff00) | (c & 0xff);
    /*cons_scroll up*/
    if ((pos / 80) >= 25)
        earlycons_scroll();
    cga_setcursor(pos);
    cga_setcursor(pos);

    return 1;
}

static size_t earlycons_write(const char *buf, size_t n)
{
    char *sbuf = (char *)buf;
    while (n && *sbuf)
    {
        earlycons_putc(*sbuf++);
        n--;
    };
    return (size_t)(sbuf - buf);
}

size_t earlycons_puts(const char *s)
{
    size_t len = strlen(s);
    return earlycons_write(s, len);
}

void earlycons_clr(void)
{
    memsetw(CGA_PTR, ((cga_attrib << 8) & 0xff00) | (' ' & 0xff), 80 * 25);
    cga_setcursor(pos = 0);
}

void earlycons_setcolor(int back, int fore)
{
    cga_save = cga_attrib;
    cga_attrib = (back << 4) | fore;
}

void earlycons_restore_color(void)
{
    cga_attrib = cga_save;
}

static int earlycons_init(void)
{
    earlycons_setcolor(CGA_BLACK, CGA_WHITE);
    earlycons_clr();
    earlycons_setpos(0);
    // void uart_init();
    // uart_init();
    return 0;
}

int earlycons_fini(void)
{
    earlycons_setcolor(CGA_BLACK, CGA_WHITE);
    use_early_cons = 0;
    return pos;
}

EARLY_INIT(early_cons, earlycons_init, NULL)