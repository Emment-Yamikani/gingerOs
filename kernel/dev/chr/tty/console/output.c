#include <bits/errno.h>
#include <arch/i386/cpu.h>
#include <mm/kalloc.h>
#include <lib/string.h>
#include <lime/string.h>
#include <locks/spinlock.h>
#include <locks/cond.h>
#include <fs/fs.h>
#include <fs/devfs.h>
#include <ds/ringbuf.h>
#include <lib/ctype.h>

static int escape_next = 0;
static uint16_t position = 0;
static uint16_t color_attr = 0;
static uint8_t *framebuffer = (uint8_t *)0xB8000;
static spinlock_t *console_lk = SPINLOCK_NEW("console_lock");
typedef uintptr_t tcflags_t;
typedef uint8_t cc_t;
#define NCCS 11

static struct console_termio
{
    tcflags_t c_iflag;
    tcflags_t c_oflag;
    tcflags_t c_cflag;
    tcflags_t c_lflag;
    cc_t c_cc[NCCS];
};


#define CGA_FRAMEBUFFER ((uint16_t *)framebuffer)
#define console_lock()  spin_lock(console_lk)
#define console_unlock() spin_unlock(console_lk)
#define console_assert_locked() spin_assert_lock(console_lk)

static void console_scroll(void)
{

}

static int console_putchar(int c)
{
    console_assert_locked();
    if ((c == '\n')){
        position += 80 - position % 80;
    }else if (c == '\t'){
        position = (position + 4) & ~3; 
    }else if (c == '\b'){
        CGA_FRAMEBUFFER[--position] = ((color_attr << 8) & 0xff00) | (' ');
    }else if (c == '\a'){

    }else if (c == '\e'){
        escape_next = 1;
    }else if (isalpha(c) || isdigit(c) || isspace(c)){
        
    }
}

int console_write(char *buf, size_t size)
{
    
}