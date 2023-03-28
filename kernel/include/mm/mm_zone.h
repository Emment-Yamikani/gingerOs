#pragma once

#include <lime/assert.h>
#include <locks/spinlock.h>
#include <mm/page.h>
#include <mm/mm_gfp.h>
#include <ds/queue.h>

typedef struct mm_zone
{
    uintptr_t   start;
    uint32_t    flags;
    page_t      *pages;
    size_t      nrpages;
    size_t      free_pages;
    queue_t     *sleep_queue;
    spinlock_t  *lock;
} mm_zone_t;

#define MM_ZONE_DMA (0)  // <  16 MiB.
#define MM_ZONE_NORM (1) // 16 MiB <= x < DEVIO_start
#define MM_ZONE_HIGH (2) //  > DEVIO_end

#define MM_ZONE_INVAL (0x0000)
#define MM_ZONE_VALID (0x0010)

#define mm_zone_assert(zone) assert(zone, "No MM_ZONE")
#define mm_zone_lock(zone) ({mm_zone_assert((zone)); spin_lock((zone)->lock); })
#define mm_zone_unlock(zone) ({mm_zone_assert((zone)); spin_unlock((zone)->lock); })
#define mm_zone_assert_locked(zone) ({mm_zone_assert(zone); spin_assert_lock((zone)->lock); })

#define mm_zone_isvalid(zone) ({mm_zone_assert_locked(zone); ((zone)->flags & MM_ZONE_VALID); })

extern mm_zone_t mm_zone[];
extern const char *str_zone[4];


/**
 * get the zone associated with physical page address 'addr'.
 * on success, a pointer to the 'locked' zone is returned else NULL is 'return'.
 * physical page address range must all be in one memory zone, else 'NULL' is returned.
*/
mm_zone_t *get_mmzone(uintptr_t addr, size_t size);

/**
 * same as 'get_mm_zone()', except, it gets an index to the physical page as a parameter.
 */
mm_zone_t *mm_zone_get(int z);

// increase the reference count of the page
int page_incr(page_t *page);
// same as page_incr(), except, it takes a physical address to the page as a parameter. 
int __page_incr(uintptr_t addr);
// returns the reference count of the page
int page_count(page_t *page);
// same as page_count(), except, it takes a physical address to the page as a parameter.
int __page_count(uintptr_t addr);

// allocate a single page.
page_t *alloc_page(gfp_mask_t gfp);
// allocate 'n' pages, where n is a power of 2.
page_t *alloc_pages(gfp_mask_t gfp, size_t order);

// get physical address of the page.
uintptr_t page_address(page_t *page);

// These two function the same way as alloc_page() and alloc_pages(),
// only differnce that they directly return the physical address of the first page allocated
uintptr_t __get_free_page(gfp_mask_t gfp);
uintptr_t __get_free_pages(gfp_mask_t gfp, size_t order);

// these two are used to reliquish a reference to the page(s).
void pages_put(page_t *page, size_t order);
void page_put(page_t *page);

// same as pages_put() and page_put(), only that they take 'addr' instead of 'page_t *'
void __pages_put(uintptr_t addr, size_t order);
void __page_put(uintptr_t addr);