#include <dev/cons.h>
#include <lib/errno.h>
#include <lib/string.h>
#include <lib/stdint.h>
#include <lib/stddef.h>
#include <arch/system.h>
#include <mm/kalloc.h>
#include <locks/spinlock.h>
#include <dev/dev.h>
#include <sys/system.h>
#include <fs/fs.h>
#include <dev/uart.h>
#include <lib/types.h>
#include <lime/module.h>
#include <fs/devfs.h>
#include <arch/boot/early.h>
#include <fs/posix.h>
#include <video/lfb_term.h>

int use_earlycon = 0;

struct dev consoledev;
#ifndef BACKSPACE
#define BACKSPACE 0x100
#endif //BACKSPACE

#define CGA_PTR ((uint16_t *)(0xC00B8000))
#define CGA_CTRL 0x3d4
#define CGA_DATA 0x3d5

static spinlock_t *cgalock = SPINLOCK_NEW("cgalock");

//position in cga buffer
static int pos = 0;
static uint16_t cga_attrib = 0;

static void cga_lock(void)
{
    spin_lock(cgalock);
}

static void cga_unlock(void)
{
    spin_unlock(cgalock);
}

static void cga_setcursor(int pos)
{
    outb(CGA_CTRL, 14);
    outb(CGA_DATA, (uint8_t)(pos >> 8));
    outb(CGA_CTRL, 15);
    outb(CGA_DATA, (uint8_t)(pos));
}

static void cons_scroll()
{
    memmove(CGA_PTR, &CGA_PTR[80], 2 * ((80 * 25) - 80));
    pos -= 80;
    memsetw(&CGA_PTR[pos], ((cga_attrib << 8) & 0xff00) | (' ' & 0xff), 80);
}

void cons_setpos(int _pos)
{
    pos = _pos;
}

void cons_putc(int c)
{
    if (!use_earlycon)
        return;
    
    if (c == BACKSPACE)
        c = '\b';

    if (use_gfx_cons)
    {
        lfb_term_putc(c);
        return;
    }

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
        cons_scroll();
    cga_setcursor(pos);
    cga_setcursor(pos);
}

size_t cons_write(const char *buf, size_t n)
{
    cga_lock();
    char *sbuf = (char *)buf;
    while (n && *sbuf)
    {
        cons_putc(*sbuf++);
        n--;
    };
    cga_unlock();
    return (size_t)(sbuf - buf);
}

size_t cons_puts(const char *s)
{
    size_t len = strlen(s);
    return cons_write(s, len);
}

void cons_clr()
{
    memsetw(CGA_PTR, ((cga_attrib << 8) & 0xff00) | (' ' & 0xff), 80 * 25);
    cga_setcursor(pos = 0);
}


static struct cons_attr
{
    uint16_t attr;
    struct cons_attr *prev, *next;
}*tail;

void cons_save(void)
{
    struct cons_attr *node;
    if (!(node = kmalloc(sizeof *node)))
        return;
    memset(node, 0, sizeof *node);
    if (tail)
    {
        tail->next = node;
        node->prev = tail;
    }
    tail = node;
    node->attr = cga_attrib;
}

void cons_setcolor(int back, int fore)
{
    cga_attrib = (back << 4) | fore;
}

void cons_restore(void)
{
    struct cons_attr *node;
    node  = tail;
    if (!node)
        return;
    if (node->prev)
    {
        node->prev->next =0;
        tail = node->prev;
        node->prev =0;
    }
    else
        tail =0;
    cga_attrib = node->attr;
    kfree(node);
}

int cons_probe()
{
    return 0;
}

int cons_mount()
{
    dev_attr_t attr = {
        .devid = *_DEVID(FS_CHRDEV, _DEV_T(DEV_CONS, 0)),
        .size = 1,
        .mask = 0220,
    };
    return devfs_mount("console", attr);
}

size_t cons_read(struct devid *dd __unused, off_t off __unused, void *buf __unused, size_t sz __unused)
{
    return 0;
}

static size_t console_write(struct devid *dd __unused, off_t off __unused, void *buf, size_t sz)
{//no out of order cons writing
    if (use_earlycon)
        sz = cons_write(buf, sz);
    else
        return 0;
    return sz;
}

int cons_open(struct devid *dd __unused, int mode __unused, ...)
{
    return 0;
}

int cons_ioctl(struct devid *dd __unused, int req __unused, void *argp __unused)
{
    return -EINVAL;
}

int cons_close(struct devid *dd __unused)
{
    return -EINVAL;
}

#include <printk.h>

static int console_init(void)
{
    pos = earlycons_fini();
    cons_setcolor(CGA_BLACK, CGA_WHITE);
    use_earlycon = 1;
    return kdev_register(&consoledev, DEV_CONS, FS_CHRDEV);
}

dev_t consoledev =
{
    .dev_name = "console",
    .dev_probe = cons_probe,
    .dev_mount = cons_mount,
    .devid = _DEV_T(DEV_CONS, 0),
    .devops =
    {
        .close = cons_close,
        .open = cons_open,
        .ioctl = cons_ioctl,
        .read = cons_read,
        .write = console_write
    },

     .fops =
    {
        .close = posix_file_close,
        .ioctl = posix_file_ioctl,
        .lseek = posix_file_lseek,
        .open = posix_file_open,
        .perm = NULL,
        .read = posix_file_read,
        .sync = NULL,
        .stat = posix_file_ffstat,
        .write = posix_file_write,
        
        .can_read = (size_t(*)(struct file *, size_t))__never,
        .can_write = (size_t(*)(struct file *, size_t))__always,
        .eof = (size_t(*)(struct file *))__never
    },
};

MODULE_INIT(cons, console_init, NULL);