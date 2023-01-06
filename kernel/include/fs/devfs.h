#ifndef FS_DEVFS_H
#define FS_DEVFS_H 1

#include <dev/dev.h>

int devfs_init();
int devfs_mount(const char *, int, struct devid);

#endif // FS_DEVFS_H