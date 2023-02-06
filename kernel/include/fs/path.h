#ifndef FS_PATH_H
#define FS_PATH_H 1

#include <lib/types.h>
#include <fs/mount.h>

typedef struct mount
{
    int flags;
    dentry_t *root;
    super_block_t *super;
} mount_t;

typedef struct path
{
    int flags;
    mount_t mnt;
    char **tokens;
    dentry_t *dentry;
} path_t;


#endif // FS_PATH_H