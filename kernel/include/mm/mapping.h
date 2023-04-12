#pragma once

#include <lib/stdint.h>
#include <lib/stddef.h>
#include <lib/types.h>
#include <ds/btree.h>
#include <ds/queue.h>

typedef struct mapping
{
    inode_t *inode;
    btree_t *btree;
    uint32_t flags;
    size_t nrpages;
    queue_t *usermaps;

    spinlock_t *lock;
} mapping_t;

#define mapping_assert(mapping)         assert(mapping, "No mapping pointer")
#define mapping_lock(mapping)           ({mapping_assert(mapping); spin_lock(mapping->lock);})
#define mapping_unlock(mapping)         ({mapping_assert(mapping); spin_unlock(mapping->lock)})
#define mapping_holding(mapping)        ({mapping_assert(mapping); spin_holding(mapping->lock);})
#define mapping_assert_locked(mapping)  ({mapping_assert(mapping); spin_assert_lock(mapping->lock);})


int mapping_new(mapping_t **pmap);
void mapping_free(mapping_t *map);
int mapping_free_page(mapping_t *map, ssize_t pgno);
int mapping_get_page(mapping_t *map, ssize_t pgno, uintptr_t *pphys, page_t **ppage);