#ifndef __INITRD__
#define __INITRD__ 1

#include <lib/stdint.h>

#define INITRD_INV 0
#define INITRD_DIR 1
#define INITRD_FILE 2

typedef struct ramfs_inode
{
    char name[32];
    int uid;
    int gid;
    int type;
    int mode;
    int offset;
    int size;
} __attribute__((packed))
ramfs_inode_t;

typedef struct ramfs_superblock
{
    uint8_t magic[10]; // "ginger1" (null terminated)
    int files_count;
    ramfs_inode_t inode[64];
} ramfs_superblock_t;

int ramfs_init();
#endif //__INITRD__