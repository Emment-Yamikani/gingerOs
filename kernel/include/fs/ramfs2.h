#pragma once

#include <lib/stdint.h>
#include <lib/stddef.h>

#define RAMFS2_INV 0
#define RAMFS2_REG 1
#define RAMFS2_DIR 2

#define __magic_len 16
#define __max_fname 64

typedef struct ramfs2_node
{
    int mode;
    int type;
    int uid, gid;
    uint32_t size;
    uint32_t offset;
    char name[__max_fname];
} ramfs2_node_t;

typedef struct ramfs2_super_header
{
    char magic[__magic_len];
    uint32_t nfile;
    uint32_t checksum; // checksum of magic, node->size(s), ramfs2_size and super_size.
    uint32_t file_size;
    uint32_t ramfs2_size;
    uint32_t super_size;
    uint32_t data_offset;
} ramfs2_super_header_t;

typedef struct ramfs2_super
{
    ramfs2_super_header_t header;
    ramfs2_node_t nodes[];
} ramfs2_super_t;

int ramfs2_init(void);
int ramfs2_validate(ramfs2_super_t *sb);
int ramfs2_find(ramfs2_super_t *super, const char *fn, ramfs2_node_t **pnode);