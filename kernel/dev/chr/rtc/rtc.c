#include <fs/fs.h>
#include <printk.h>
#include <dev/dev.h>
#include <lib/stdint.h>
#include <lib/stddef.h>
#include <sys/thread.h>
#include <sys/fcntl.h>
#include <arch/i386/cpu.h>
#include <sys/thread.h>
#include <arch/system.h>
#include <mm/kalloc.h>
#include <arch/i386/traps.h>
#include <locks/cond.h>
#include <locks/spinlock.h>
#include <arch/chipset/chipset.h>
#include <lime/module.h>
#include <lib/string.h>
#include <fs/devfs.h>
#include <fs/posix.h>
#include <mm/kalloc.h>

#define RTC_CMD 0x70
#define RTC_IO 0x71

struct dev rtc0dev;
spinlock_t *rtclock = SPINLOCK_NEW("rtclock");

static long rtc_secs = 0;
static long rtc_ticks = 0;

static cond_t *rtc_event = NULL;

static void rtc_lock(void)
{
    spin_lock(rtclock);
}

static void rtc_unlock(void)
{
    spin_unlock(rtclock);
}

long rtc_getsec(void)
{
    long sec = 0;
    rtc_lock();
    sec = rtc_secs;
    rtc_unlock();
    return sec;
}

int rtc_wait(void)
{
    return cond_wait(rtc_event);
}

int rtc_init(void)
{
    int err = 0;
    if ((err = cond_init(NULL, "rtc-event", &rtc_event)))
        return err;
    return kdev_register(&rtc0dev, DEV_RTC0, FS_CHRDEV);
}

void rtc_intr(void)
{
    if (!((++rtc_ticks) % 2))
    {
        ++rtc_secs;
        // printk("%lds\r", ++rtc_secs);
        cond_broadcast(rtc_event);
    }

    outb(RTC_CMD, 0x0C);
    inb(RTC_IO);
}

int rtc0_probe(void)
{
    pic_enable(IRQ_RTC);
    ioapic_enable(IRQ_RTC, cpuid());

    outb(RTC_CMD, 0x8A);
    uint8_t value = inb(RTC_IO);
    outb(RTC_CMD, 0x8A);
    outb(RTC_IO, ((value & 0xF0) | 0x0F));

    outb(RTC_CMD, 0X8B);
    value = inb(RTC_IO);
    outb(RTC_CMD, 0x8B);
    outb(RTC_IO, (value | 0x40));

    outb(RTC_CMD, 0x0C);
    inb(RTC_IO);

    return 0;
}

int rtc0_mount(void)
{
    return devfs_mount("rtc0", 0444, *_DEVID(FS_CHRDEV, _DEV_T(DEV_RTC0, 0)));
}

int rtc0_open(struct devid *dd __unused, int oflags __unused, ...)
{
    return 0;
}

int rtc0_close(struct devid *dd __unused)
{
    return 0;
}

size_t rtc0_read(struct devid *dd __unused, off_t offset __unused, void *buf, size_t sz)
{
    size_t size = MIN(sizeof(rtc_secs), sz);
    rtc_lock();
    memcpy(buf, &rtc_secs, size);
    rtc_unlock();
    return size;
}

size_t rtc0_write(struct devid *dd __unused, off_t offset __unused, void *buf __unused, size_t sz __unused)
{
    return 0;
}

int rtc0_ioctl(struct devid *dd __unused, int request __unused, void *argp __unused)
{
    return 0;
}

struct dev rtc0dev = 
{
    .dev_name = "rtc0",
    .dev_probe = rtc0_probe,
    .dev_mount = rtc0_mount,
    .devid = _DEV_T(DEV_RTC0, 0),
    .devops = 
    {
        .open = rtc0_open,
        .read = rtc0_read,
        .write = rtc0_write,
        .ioctl = rtc0_ioctl,
        .close = rtc0_close
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
        .can_write = (size_t(*)(struct file *, size_t))__always,
        .eof = (size_t(*)(struct file *))__never
    },
};

MODULE_INIT(rtc0, rtc_init, NULL);