#include <dev/ps2mouse.h>
#include <bits/errno.h>
#include <arch/system.h>
#include <printk.h>
#include <arch/chipset/chipset.h>
#include <locks/cond.h>
#include <locks/spinlock.h>
#include <dev/dev.h>
#include <fs/fs.h>
#include <fs/posix.h>
#include <sys/fcntl.h>
#include <lib/string.h>
#include <lime/module.h>
#include <fs/devfs.h>

#define PS2MOUSE_DATA   0x60
#define PS2MOUSE_CMD    0x64

#define PS2MOUSE_RESET  0xFF
#define PS2MOUSE_ACK    0xFA
#define PS2MOUSE_SETDEFAULT 0xF6
#define PS2_ENABLE      0xF4
#define PS2MOUSE_SAMPLE_RATE    0xF3
#define PS2MOUSE_GETID  0xF2
#define PS2MOUSE_REQ_STATUS 0xE9
#define PS2MOUSE_SET_RESOLUTION 0xE8

#define PS2MOUSE_WAIT_READ  0
#define PS2MOUSE_WAIT_WRITE 1

static struct dev ps2mousedev;

static void mouse_wait(int which)
{
    int timeout = 100000;
    switch (which)
    {
    /*wait for write */
    case PS2MOUSE_WAIT_WRITE:
        while (--timeout)
            if (!(inb(PS2MOUSE_CMD) & 2))
                return;
        break;
        /*wait for read */
    case PS2MOUSE_WAIT_READ:
        while (--timeout)
            if ((inb(PS2MOUSE_CMD) & 1))
                return;
        break;
    default:
        break;
    }
}

static void mouse_write(uint8_t data)
{
    mouse_wait(PS2MOUSE_WAIT_WRITE);
    outb(PS2MOUSE_CMD, 0xD4);
    mouse_wait(PS2MOUSE_WAIT_WRITE);
    outb(PS2MOUSE_DATA, data);
}

static uint8_t mouse_read(void)
{
    mouse_wait(PS2MOUSE_WAIT_READ);
    return inb(PS2MOUSE_DATA);
}

uint8_t identify(void)
{
    mouse_write(PS2MOUSE_GETID);
    return inb(PS2MOUSE_DATA);
}

int ps2mouse_enable(void)
{
    mouse_wait(PS2MOUSE_WAIT_WRITE);
    outb(PS2MOUSE_CMD, 0xA8);
    
    mouse_wait(PS2MOUSE_WAIT_WRITE);
    outb(PS2MOUSE_CMD, 0x20);
    uint8_t status = inb(PS2MOUSE_DATA) | 2;
    mouse_wait(PS2MOUSE_WAIT_WRITE);
    outb(PS2MOUSE_CMD, 0x60);
    outb(PS2MOUSE_DATA, status);

    mouse_write(PS2MOUSE_SETDEFAULT);
    mouse_read();

    mouse_write(PS2_ENABLE);
    mouse_read();

    pic_enable(IRQ_MOUSE);
    ioapic_enable(12, 0);
    return 0;
}


static cond_t *ps2mouse_event = NULL;
static spinlock_t *ps2mouselock = SPINLOCK_NEW("ps2mouselock");

mouse_packet_t ps2mouse_packet;
int mouse_x = 0;
int mouse_y = 0;
uint8_t mouse_cycle =0;

mouse_data_t mouse_data;

#define PACKETS_IN_PIPE 1024
#define DISCARD_POINT 32

#define MOUSE_IRQ 12

#define MOUSE_PORT   0x60
#define MOUSE_STATUS 0x64
#define MOUSE_ABIT   0x02
#define MOUSE_BBIT   0x01
#define MOUSE_WRITE  0xD4
#define MOUSE_F_BIT  0x20
#define MOUSE_V_BIT  0x08

static uint8_t mouse_byte[3] = {0};

#define ps2mouse_lock() spin_lock(ps2mouselock)

#define ps2mouse_unlock() spin_unlock(ps2mouselock)

void ps2mouse_handler(void)
{
    uint8_t status = inb(MOUSE_STATUS);
    while (status & MOUSE_BBIT)
    {
        int8_t mouse_in = inb(MOUSE_PORT);
        if (status & MOUSE_F_BIT)
        {
            switch (mouse_cycle)
            {
            case 0:
                mouse_byte[0] = mouse_in;
                if (!(mouse_in & MOUSE_V_BIT))
                    return;
                ++mouse_cycle;
                break;
            case 1:
                mouse_byte[1] = mouse_in;
                ++mouse_cycle;
                break;
            case 2:
                mouse_byte[2] = mouse_in;
                /* We now have a full mouse packet ready to use */
                if (mouse_byte[0] & 0x80 || mouse_byte[0] & 0x40)
                {
                    /* x/y overflow? bad packet! */
                    break;
                }
                mouse_device_packet_t packet;
                packet.magic = MOUSE_MAGIC;
                packet.x_difference = mouse_byte[1];
                packet.y_difference = mouse_byte[2];
                packet.buttons = 0;
                if (mouse_byte[0] & 0x01)
                {
                    packet.buttons |= LEFT_CLICK;
                }
                if (mouse_byte[0] & 0x02)
                {
                    packet.buttons |= RIGHT_CLICK;
                }
                if (mouse_byte[0] & 0x04)
                {
                    packet.buttons |= MIDDLE_CLICK;
                }
                mouse_cycle = 0;

                ps2mouse_lock();
                mouse_data.buttons = packet.buttons;
                mouse_data.x = packet.x_difference;
                mouse_data.y = packet.y_difference;
                ps2mouse_unlock();

                cond_broadcast(ps2mouse_event);
                break;
            }
        }
        status = inb(MOUSE_STATUS);
    }
}

int ps2mouse_mount(void)
{
    dev_attr_t attr = {
        .devid = *_DEVID(FS_CHRDEV, _DEV_T(DEV_MOUSE, 0)),
        .size = sizeof (mouse_data_t),
        .mask = 0444,
    };
    return devfs_mount("event0", attr);
}

int ps2mouse_open(struct devid *dd __unused, int mode __unused, ...)
{
    return 0;
}

int ps2mouse_close(struct devid *dd __unused)
{
    return -EINVAL;
}

int ps2mouse_ioctl(struct devid *dd __unused, int req __unused, void *argp __unused)
{
    return -EINVAL;
}

size_t ps2mouse_read(struct devid *dd __unused, off_t off __unused, void *buf __unused, size_t n)
{
    size_t size = 0;
    cond_wait(ps2mouse_event);
    ps2mouse_lock();
    size = MIN(n, sizeof mouse_data - off);
    size = (size_t)(memcpy(buf, (void *)(&mouse_data), n) - buf);
    ps2mouse_unlock();
    mouse_data.buttons = 0;
    mouse_data.x =0;
    mouse_data.y =0;
    mouse_data.z =0;
    return size;
}

size_t ps2mouse_write(struct devid *dd __unused, off_t off __unused, void *buf __unused, size_t sz __unused)
{
    return 0;
}

int ps2mouse_probe(void)
{
    return 0;
}

int ps2mouse_init(void)
{
    int ret = 0;
    if ((ret = cond_init(ps2mouse_event, "ps2mouse-event", &ps2mouse_event)))
        return ret;
    ps2mouse_enable();
    return kdev_register(&ps2mousedev, DEV_MOUSE, FS_CHRDEV);
}

static struct dev ps2mousedev = 
{
    .dev_name = "event0",
    .dev_probe = ps2mouse_probe,
    .dev_mount = ps2mouse_mount,
    .devid = _DEV_T(DEV_MOUSE, 0),
    .devops = 
    {
        .open = ps2mouse_open,
        .read = ps2mouse_read,
        .write = ps2mouse_write,
        .ioctl = ps2mouse_ioctl,
        .close = ps2mouse_close
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

MODULE_INIT(ps2mouse, ps2mouse_init, NULL);