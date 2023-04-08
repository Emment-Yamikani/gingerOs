#pragma once

#include <ds/btree.h>
#include <lib/stdint.h>
#include <lib/stddef.h>
#include <locks/spinlock.h>
#include <lib/types.h>
#include <lime/assert.h>
#include <mm/mapping.h>
#include <mm/page_flags.h>

typedef struct page
{
    page_flags_t flags;
    mapping_t *mapping;
    atomic_t ref_count;
    uintptr_t virtual; // virtual addr
} page_t;

#define page_flags(page) (page->flags)
#define page_user(page) ({page_flags(page).user;})         // user
#define page_exec(page) ({page_flags(page).exec;})         // exec'able
#define page_read(page) ({page_flags(page).read;})         // readable
#define page_write(page) ({page_flags(page).write;})       // writeable
#define page_dirty(page) ({page_flags(page).dirty;})       // dirty
#define page_valid(page) ({page_flags(page).valid;})       // valid
#define page_swapped(page) ({page_flags(page).swapped;})   // swapped
#define page_can_swap(page) ({page_flags(page).can_swap;}) // can_swap

