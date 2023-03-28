#include <mm/mm_zone.h>
#include <mm/pmm.h>
#include <sys/kthread.h>

uintptr_t mm_alloc(void);
void mm_free(uintptr_t);
size_t mem_free(void);
size_t mem_used(void);

struct pmman pmman = {
    .free = mm_free,
    .alloc = mm_alloc,
    .mem_free = mem_free,
    .mem_used = mem_used,
    .init = physical_memory,
};

page_t *alloc_pages(gfp_mask_t gfp, size_t order)
{
    size_t index = 0;
    size_t start = 0;
    page_t *page = NULL;
    mm_zone_t *zone = NULL;
    size_t alloced = 0, npages = _BS(order);
    int where = !(gfp & 0x0F) ? MM_ZONE_NORM : (gfp & 0x0F) - 1;

    zone = mm_zone_get(where);

    loop()
    {
        for (start = index = 0; index < zone->nrpages; start = ++index)
        {
            if (zone->pages[index].ref_count)
            {
                alloced = 0;
                continue;
            }

            if ((++alloced) == npages)
                goto done;
        }
        if ((!(gfp & GFP_WAIT) && !(gfp & GFP_RETRY)))
            goto error;
    }

done:
    page = &zone->pages[start];
    for (size_t count = 0; count < npages; ++count)
    {
        page[count].flags.mm_zone = zone - mm_zone;
        page[count].ref_count++;
        zone->free_pages--;
    }
    mm_zone_unlock(zone);
    return page;
error:
    mm_zone_unlock(zone);
    return NULL;
}

page_t *alloc_page(gfp_mask_t gfp)
{
    return alloc_pages(gfp, 0);
}

uintptr_t page_address(page_t *page)
{
    long index = 0;
    uintptr_t addr = 0;
    mm_zone_t *zone = NULL;

    if (!page)
        return 0;

    if (!(zone = mm_zone_get(page->flags.mm_zone)))
        return 0;

    index = page - zone->pages;

    if ((index < 0) || (index > (long)zone->nrpages))
    {
        mm_zone_unlock(zone);
        return 0;
    }

    addr = zone->start + (index * PAGESZ);
    mm_zone_unlock(zone);
    return addr;
}

int __page_incr(uintptr_t addr)
{
    size_t refcnt = 0;
    mm_zone_t *zone = NULL;

    if (!addr)
        panic("%s(%p)???\n", __func__, addr);
    if (!(zone = get_mmzone(addr, PAGESZ)))
        panic("%s(%p)???\n", __func__, addr);
    refcnt = atomic_incr(&zone->pages[(addr - zone->start) / PAGESZ].ref_count);
    mm_zone_unlock(zone);
    return refcnt;
}

int page_incr(page_t *page)
{
    return __page_incr(page_address(page));
}

int __page_count(uintptr_t addr)
{
    size_t refcnt = 0;
    mm_zone_t *zone = NULL;

    if (!addr)
        panic("%s(%p)???\n", __func__, addr);
    if (!(zone = get_mmzone(addr, PAGESZ)))
        panic("%s(%p)???\n", __func__, addr);
    refcnt = atomic_read(&zone->pages[(addr - zone->start) / PAGESZ].ref_count);
    mm_zone_unlock(zone);
    return refcnt;
}

int page_count(page_t *page)
{
    return __page_count(-page_address(page));
}

uintptr_t __get_free_pages(gfp_mask_t gfp, size_t order)
{
    return page_address(alloc_pages(gfp, order));
}

uintptr_t __get_free_page(gfp_mask_t gfp)
{
    return __get_free_pages(gfp, 0);
}

void pages_put(page_t *page, size_t order)
{
    __pages_put(page_address(page), order);
}

void page_put(page_t *page)
{
    pages_put(page, 0);
}

void __pages_put(uintptr_t addr, size_t order)
{
    page_t *page = NULL;
    mm_zone_t *zone = NULL;
    size_t npages = _BS(order);

    if (!addr)
        panic("%s(%p, %d)???\n", __func__, addr, order);

    if (!(zone = get_mmzone(addr, npages * PAGESZ)))
        panic("%s(%p, %d)???\n", __func__, addr, order);
    
    page = &zone->pages[(addr - zone->start) / PAGESZ];
    for (size_t pages = 0; pages < npages; ++pages, ++page)
    {
        if (atomic_read(&page->ref_count) == 0)
            continue;
        atomic_decr(&page->ref_count);
        if (atomic_read(&page->ref_count) == 0)
        {
            *page = (page_t){0};
            zone->free_pages++;
        }
    }
    mm_zone_unlock(zone);
}

void __page_put(uintptr_t addr)
{
    __pages_put(addr, 0);
}

uintptr_t mm_alloc(void)
{
    return __get_free_page(GFP_NORMAL);
}

void mm_free(uintptr_t addr)
{
    //printk("%s(%p)\n", __func__, addr);
    __page_put(addr);
}

size_t mem_free(void)
{
    size_t size = 0;
    for (size_t zone = MM_ZONE_DMA; zone < 3; ++zone)
    {
        mm_zone_lock(&mm_zone[zone]);
        if (mm_zone_isvalid(&mm_zone[zone]))
            size += mm_zone[zone].free_pages * PAGESZ;
        mm_zone_unlock(&mm_zone[zone]);
    }
    return (size / 1024);
}

size_t mem_used(void)
{
    size_t size = 0;
    for (size_t zone = MM_ZONE_DMA; zone < 3; ++zone)
    {
        mm_zone_lock(&mm_zone[zone]);
        if (mm_zone_isvalid(&mm_zone[zone]))
            size += (mm_zone[zone].nrpages - mm_zone[zone].free_pages) * PAGESZ;
        mm_zone_unlock(&mm_zone[zone]);
    }
    return (size / 1024);
}

void *memory(void *arg)
{
    
    return arg;
}

BUILTIN_THREAD(memory, memory, NULL);