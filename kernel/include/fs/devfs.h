#ifndef FS_DEVFS_H
#define FS_DEVFS_H 1

#include <dev/dev.h>

typedef struct dev_attr
{
    int mask;
    size_t size;
    struct devid devid;
}dev_attr_t;

int devfs_init();
int devfs_mount(const char *, dev_attr_t dev_attr);

#endif // FS_DEVFS_H