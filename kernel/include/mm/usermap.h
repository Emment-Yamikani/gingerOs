#pragma once

#include <mm/mmap.h>
#include <lib/stdint.h>
#include <arch/i386/paging.h>
#include <mm/page.h>
#include <fs/fs.h>
#include <mm/mapping.h>
#include <locks/spinlock.h>


typedef struct usermap
{
    mmap_t      *mmap;      // Process memory map.
    ssize_t     pageno;     // Page number in the file mapping.
    mapping_t   *mapping;   // Mapping to which this shared page points.
    uintptr_t   vaddr;      // Virtual address in the memory map of process.
} usermap_t;

typedef struct shared_mapping
{
    mmap_t      *mmap;      // Process memory map.
    long        refcnt;     // Reference count.
    ssize_t     pageno;     // Page number in the file mapping.
    inode_t     *inode;     // File inode to which we are sharing a mapping.
    uintptr_t   vaddr;      // Virtual address in the memory map of process.
    spinlock_t  *lock;      // Lock to protect this shared_mapping struct
} shared_mapping_t;

usermap_t *alloc_usermap(void);
void free_usermap(usermap_t *);

int shared_mapping_alloc(mmap_t *mmap, inode_t *inode, ssize_t pageno, uintptr_t virtaddr, shared_mapping_t **pshared_mapping);