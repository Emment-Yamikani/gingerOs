#include <arch/system.h>
#include <bits/errno.h>
#include <dev/dev.h>
#include <fs/fs.h>
#include "kbd.h"
#include <locks/cond.h>
#include <locks/spinlock.h>
#include <sys/system.h>
#include <dev/cons.h>
#include <arch/chipset/chipset.h>
#include <lime/module.h>
#include <sys/thread.h>
#include <fs/devfs.h>
#include <fs/posix.h>

dev_t kbd0dev;

#ifndef BACKSPACE
#define BACKSPACE 0x100
#endif // BACKSPACE

int kbdgetc(void)
{
    static uint32_t shift;
    static uint8_t *charcode[4] = {
        normalmap, shiftmap, ctlmap, ctlmap};
    uint32_t st, data, c;

    st = inb(KBSTATP);
    if ((st & KBS_DIB) == 0)
        return -1;
    data = inb(KBDATAP);

    if (data == 0xE0)
    {
        shift |= E0ESC;
        return 0;
    }
    else if (data & 0x80)
    {
        // Key released
        data = (shift & E0ESC ? data : data & 0x7F);
        shift &= ~(shiftcode[data] | E0ESC);
        return 0;
    }
    else if (shift & E0ESC)
    {
        // Last character was an E0 escape; or with 0x80
        data |= 0x80;
        shift &= ~E0ESC;
    }

    shift |= shiftcode[data];
    shift ^= togglecode[data];
    c = charcode[shift & (CTL | SHIFT)][data];
    if (shift & CAPSLOCK)
    {
        if ('a' <= c && c <= 'z')
            c += 'A' - 'a';
        else if ('A' <= c && c <= 'Z')
            c += 'a' - 'A';
    }
    return c;
}

spinlock_t *inputlock = SPINLOCK_NEW("kbdlock");
#define INPUT_BUF 256

struct
{
    int buf[INPUT_BUF];
    uint32_t r; // Read index
    uint32_t w; // Write index
    uint32_t e; // Edit index
} input;

static cond_t *kbd0_event = 0;

static void kbd0_lock(void)
{
    spin_lock(inputlock);
}

static void kbd0_unlock(void)
{
    spin_unlock(inputlock);
}

void __kbdintr(int (*getc)(void))
{
    int c, __unused doprocdump = 0;

    kbd0_lock();
    while ((c = getc()) >= 0)
    {
        switch (c)
        {
        case C('P'):        // Process listing.
            doprocdump = 1; // procdump() locks kbd.lock indirectly; invoke later
            break;
        case C('U'): // Kill line.
            while (input.e != input.w &&
                   input.buf[(input.e - 1) % INPUT_BUF] != '\n')
            {
                input.e--;
                cons_putc(BACKSPACE);
            }
            break;
        case C('H'):
        case '\x7f': // Backspace
            if (input.e != input.w)
            {
                input.e--;
                cons_putc(BACKSPACE);
            }
            break;
        case C('Q'): // clear screen(mega)
            cons_clr();
            break;
        case C('M'): // memory usage info
            memory_usage();
            break;
        default:
            if (c != 0 && input.e - input.r < INPUT_BUF)
            {
                c = (c == '\r') ? '\n' : c;
                input.buf[input.e++ % INPUT_BUF] = c;
                cons_putc(c);
                if (c == '\n' || c == C('D') || input.e == (input.r + INPUT_BUF))
                {
                    input.w = input.e;
                    cond_signal(kbd0_event);
                }
            }
            break;
        }
    }
    kbd0_unlock();
    // if (doprocdump)
    // proc_dump(); // now call procdump() wo. kbd.lock held
}

void kbd_intr(void)
{
    __kbdintr(kbdgetc);
}

int kbd0_probe()
{
    int err = 0;
    if ((err = cond_init(NULL, "kbd0_even", &kbd0_event)))
        return err;
    int st = inb(KBSTATP);
    if ((st & KBS_DIB) == 0)
        goto ok;
    inb(KBDATAP);
    ok:
    pic_enable(IRQ_KBD);
    ioapic_enable(IRQ_KBD, cpuid());
    return 0;
}

int kbd0_mount(void)
{
    dev_attr_t attr = {
        .devid = *_DEVID(FS_CHRDEV, _DEV_T(DEV_KBD, 0)),
        .size = 1,
        .mask = 0444,
    };
    return devfs_mount("kbd0", attr);
}

int kbd0_open(struct devid *dd __unused, int mode __unused, ...)
{
    return 0;
}

int kbd0_close(struct devid *dd __unused)
{
    return -EINVAL;
}

int kbd0_ioctl(struct devid *dd __unused, int req __unused, void *argp __unused)
{
    return -EINVAL;
}

size_t kbd0_read(struct devid *dd __unused, off_t offset __unused, void *__buf, size_t sz)
{
    int c = 0;
    size_t target = sz;
    char *buf = __buf;

    kbd0_lock();
    while (sz > 0)
    {
        if (atomic_read(&current->t_killed))
        {
            kbd0_unlock();
            return -EINTR;
        }

        while (input.r == input.w)
        {
            if (atomic_read(&current->t_killed))
            {
                kbd0_unlock();
                return -EINTR;
            }

            kbd0_unlock();
            cond_wait(kbd0_event);
            kbd0_lock();
        }

        c = input.buf[input.r++ % INPUT_BUF];
        if (c == C('D'))
        {
            if (sz < target)
                input.r--;
            break;
        }
        *buf++ = c;
        --sz;
        if (c == '\n')
            break;
    }
    kbd0_unlock();

    return target - sz;
}

size_t kbd0_write(struct devid *dd __unused, off_t offset __unused, void *buf __unused, size_t sz __unused)
{
    return 0;
}

int kbd0_init(void)
{
    return kdev_register(&kbd0dev, DEV_KBD, FS_CHRDEV);
}

struct dev kbd0dev = 
{
    .dev_name = "kbd0",
    .dev_probe = kbd0_probe,
    .dev_mount = kbd0_mount,
    .devid = _DEV_T(DEV_KBD, 0),
    .devops = 
    {
        .open = kbd0_open,
        .read = kbd0_read,
        .write = kbd0_write,
        .ioctl = kbd0_ioctl,
        .close = kbd0_close
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
        
        .can_read = (size_t(*)(struct file *, size_t))__always,
        .can_write = (size_t(*)(struct file *, size_t))__never,
        .eof = (size_t(*)(struct file *))__never
    },
};

MODULE_INIT(kbd, kbd0_init, NULL);