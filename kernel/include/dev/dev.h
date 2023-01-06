#ifndef DEV_DEV_H
#define DEV_DEV_H 1

#include <lib/types.h>
#include <lib/stddef.h>
#include <lib/stdint.h>

#define DEV_KBD     0
#define DEV_UART    4
#define DEV_CONS    5
#define DEV_HPET    10
#define DEV_FB0     29
#define DEV_RTC0    249

#define DEV_RAMDISK 0

struct devid
{
    int     dev_type;
    uint8_t dev_minor;
    uint8_t dev_major;
};

typedef struct devops
{
    int (*open)(struct devid *, int, ...);
    int (*close)(struct devid *);
    int (*ioctl)(struct devid *, int, void *);
    int (*lseek)(struct devid *, off_t, int);
    size_t (*read)(struct devid *, off_t, void *, size_t);
    size_t (*write)(struct devid *, off_t, void *, size_t);
} devops_t;

typedef struct dev
{
    devid_t devid;
    char *dev_name;
    int (*dev_probe)();
    int (*dev_mount)();
    devops_t devops;
} dev_t;

#define DEV(d, T)     \
dev_t d##dev =              \
{                           \
    .dev_name = #d,       \
    .dev_probe = d##_probe, \
    .dev_mount = d##_mount, \
    .devid = T,             \
    .devops =               \
    {                       \
        .open = d##_open,   \
        .close = d##_close, \
        .read = d##_read,   \
        .write = d##_write, \
        .ioctl = d##_ioctl  \
    }                       \
}

#define _DEV_T(major, minor) ((devid_t)(((minor & 0xff) << 8) | (major & 0xff)))
#define _DEV_MAJOR(devid) ((uint8_t)(devid & 0xff))
#define _DEV_MINOR(devid) ((uint8_t)((devid >> 8) & 0xff))
#define _DEVID(type, rdev) (&(struct devid){.dev_major = _DEV_MAJOR(rdev), .dev_minor = _DEV_MINOR(rdev), .dev_type = type})
#define _INODE_DEV(inode) _DEVID(inode->i_type, inode->i_rdev)

int dev_init(void);
dev_t *kdev_get(struct devid *dd);
int kdev_register(dev_t *dev, uint8_t major, uint8_t type);

int kdev_close(struct devid *dd);
int kdev_open(struct devid *dd, int, ...);
int kdev_ioctl(struct devid *dd, int request, void *argp);
size_t kdev_lseek(struct devid *dd, off_t offset, int whence);
size_t kdev_read(struct devid *dd, off_t offset, void *buf, size_t sz);
size_t kdev_write(struct devid *dd, off_t offset, void *buf, size_t sz);

#endif // DEV_DEV_H