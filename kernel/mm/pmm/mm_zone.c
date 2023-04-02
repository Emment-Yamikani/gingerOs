#include <bits/errno.h>
#include <arch/boot/boot.h>
#include <lib/string.h>
#include <mm/mm_zone.h>
#include <mm/page.h>
#include <printk.h>
#include <arch/i386/paging.h>
#include <sys/sched.h>
#include <sys/thread.h>
#include <sys/proc.h>
#include <arch/i386/cpu.h>
#include <arch/i386/paging.h>
#include <mm/pmm.h>

const char *str_zone[4] = {
    [MM_ZONE_DMA] = "ZONE_DMA",
    [MM_ZONE_NORM] = "ZONE_NORMAL",
    [MM_ZONE_HIGH] = "ZONE_HIGHMEM",
    [3] = NULL,
};

/**
 * Array of all memory zone descriptors
 */
mm_zone_t mm_zone[] = {
    (mm_zone_t){
        .nrpages = 0,
        .pages = NULL,
        .flags = MM_ZONE_INVAL,
        .lock = SPINLOCK_NEW("MM_ZONE_DMA"),
        .sleep_queue = QUEUE_NEW("MM_ZONE_DMA"),
    },
    (mm_zone_t){
        .nrpages = 0,
        .pages = NULL,
        .flags = MM_ZONE_INVAL,
        .lock = SPINLOCK_NEW("MM_ZONE_NORMAL"),
        .sleep_queue = QUEUE_NEW("MM_ZONE_NORMAL"),
    },
    (mm_zone_t){
        .nrpages = 0,
        .pages = NULL,
        .flags = MM_ZONE_INVAL,
        .lock = SPINLOCK_NEW("MM_ZONE_HIGH"),
        .sleep_queue = QUEUE_NEW("MM_ZONE_HIGH"),
    },
};

/**
 * Enumerates all the available memory on the machine
 * and divides it into memory zones.
 * If RAM size if < 16MiB, enumerate_zones() panics.
 *
 * Every zone made is marked 'VALID'.
 *
 * 1. MM_ZONE_DMA is all memory < 16MiB.
 * 2. MM_ZONE_NORM if memory between 16MiB and MMAP_DEV(0xFE000000).
 * 3. MM_ZONE_HIGH is all available memory above 4GiB.
 *
 * The array of pages if made availble starting @ 16MiB.
 */
static int enumarate_zones(void)
{
    uintptr_t start = 0;
    size_t size = 0x1000000;
    uint64_t memsize = bootinfo.pmemsize;
    start = bootinfo.mods[bootinfo.mods_count - 1].addr;
    start += bootinfo.mods[bootinfo.mods_count - 1].size;
    void *page_array = (void *)PGROUNDUP(start);
    start = 0;

    if ((bootinfo.pmemsize / 1024) < 16)
        panic("RAM size < 16MiB\n");

    mm_zone[MM_ZONE_DMA].pages = page_array;
    mm_zone[MM_ZONE_DMA].flags = MM_ZONE_VALID;
    mm_zone[MM_ZONE_DMA].start = start;
    mm_zone[MM_ZONE_DMA].nrpages = size / PAGESZ;
    mm_zone[MM_ZONE_DMA].free_pages = size / PAGESZ;

    start += size;
    page_array += mm_zone[MM_ZONE_DMA].nrpages * sizeof(page_t);
    memsize -= size / 1024;
    size = (memsize >= (MMAP_DEVADDR - size) / 1024) ? ((MMAP_DEVADDR - size) / 1024) : memsize;
    size *= 1024;

    mm_zone[MM_ZONE_NORM].pages = page_array;
    mm_zone[MM_ZONE_NORM].flags = MM_ZONE_VALID;
    mm_zone[MM_ZONE_NORM].start = start;
    mm_zone[MM_ZONE_NORM].nrpages = size / PAGESZ;
    mm_zone[MM_ZONE_NORM].free_pages = size / PAGESZ;

    memsize -= (0x2000000 + size) / 1024;
    if ((int64_t)memsize <= 0)
        goto done;

#ifdef ARCH_x86_64
    start = 0x100000000;
    page_array += mm_zone[MM_ZONE_NORM].nrpages * sizeof(page_t);
    mm_zone[MM_ZONE_HIGH].pages = page_array;
    mm_zone[MM_ZONE_HIGH].flags = MM_ZONE_VALID;
    mm_zone[MM_ZONE_HIGH].start = start;
    mm_zone[MM_ZONE_HIGH].nrpages = memsize / 4;
    mm_zone[MM_ZONE_HIGHfree_pageses = memsize / 4;
#endif

done:
    return 0;
}

static int map_zones(void)
{
    int index = 0;
    size_t size = 0;
    uintptr_t start = 0;
    mm_zone_t *zone = NULL;
    uintptr_t tablesz = 0;
    uintptr_t phys_start = 0x1000000;
    uintptr_t ptable = 0, vtable = 0;

    size = mm_zone[MM_ZONE_DMA].nrpages * sizeof(page_t);
    size += mm_zone[MM_ZONE_NORM].nrpages * sizeof(page_t);

    mm_zone_lock(&mm_zone[MM_ZONE_HIGH]);
    if (mm_zone_isvalid(&mm_zone[MM_ZONE_HIGH]))
        size += mm_zone[MM_ZONE_HIGH].nrpages * sizeof(page_t);
    mm_zone_unlock(&mm_zone[MM_ZONE_HIGH]);


    start = (uintptr_t)mm_zone[MM_ZONE_DMA].pages;
    ptable = VMA_LOW((vtable = (start + size)));
    tablesz = NTABLE(size) * PAGESZ;

    for (int j = 0; j < NTABLE(size); ++j, ptable += PAGESZ, vtable += PAGESZ)
    {
        PDE(V_PDI(vtable))->raw = (ptable | VM_KRW | VM_PWT);
        paging_invlpg((uintptr_t)PTE(V_PDI(vtable), 0));
        for (int i = 0; i < 1024; ++i)
            PTE(V_PDI(vtable), i)->raw = 0;
        paging_invlpg((uintptr_t)PTE(V_PDI(vtable), 0));
    }

    paging_identity_map(phys_start, start, size, VM_PWT | VM_KRW);
    memset((void *)start, 0, size);

    mm_zone[MM_ZONE_DMA].free_pages -= 1;
    mm_zone[MM_ZONE_DMA].pages[0].ref_count = 1;
    mm_zone[MM_ZONE_DMA].pages[index].flags.mm_zone = 0;

    size += tablesz;
    zone = get_mmzone(phys_start, size);
    mm_zone_assert(zone);

    index = (phys_start - zone->start) / PAGESZ;
    for (int j = 0; j < (int)NPAGE(size); ++j, ++index)
    {
        atomic_write(&zone->pages[index].ref_count, 1);
        zone->pages[index].flags.mm_zone = zone - mm_zone;
        zone->pages[index].flags.raw |= VM_KRW;
    }

    //printk("pages: %p\n", start + size);
    zone->free_pages -= NPAGE(size);

    mm_zone_unlock(zone);
    return 0;
}

int physical_memory(void)
{
    int err = 0;
    int index = 0;
    size_t size = 0;
    uintptr_t addr = 0;
    mm_zone_t *zone = NULL;
    typeof(*bootinfo.mmap) *map = bootinfo.mmap;
    typeof(*bootinfo.mods) *module = bootinfo.mods;

    if ((err = enumarate_zones()))
        return err;

    if ((err = map_zones()))
        return err;

    for (int i = 0; i < bootinfo.mmap_count; ++i)
    {
        addr = PGROUND(map[i].addr);
        size = PGROUNDUP(map[i].size);

        if (map[i].type != MULTIBOOT_MEMORY_AVAILABLE)
        {
            zone = get_mmzone(addr, size);
            if (zone)
            {
                index = (addr - zone->start) / PAGESZ;
                for (int j = 0; j < (int)NPAGE(size); ++j, ++index)
                {
                    atomic_write(&zone->pages[index].ref_count, 1);
                    zone->pages[index].flags.mm_zone = zone - mm_zone;
                    zone->pages[index].flags.raw |= VM_KRW;
                }

                zone->free_pages -= NPAGE(size);
                mm_zone_unlock(zone);
            }

            //printk("addr: %p, size: %d, zone: %p is not available\n", addr, size, zone ? zone->start : 0);
            if (!paging_getmapping(addr) && (addr < MMAP_DEVADDR))
                paging_identity_map(addr, addr, size, VM_KRW | VM_PWT | VM_PCD);
        }
    }

    for (int i = 0; i < bootinfo.mods_count; ++i)
    {
        addr = PGROUND(VMA_LOW(module[i].addr));
        size = PGROUNDUP(module[i].size);
        index = addr / PAGESZ;
        for (int j = 0; j < (int)NPAGE(size); ++j, ++index)
        {
            mm_zone[MM_ZONE_DMA].pages[index].ref_count = 1;
            mm_zone[MM_ZONE_DMA].pages[index].flags.mm_zone = 0;
            mm_zone[MM_ZONE_DMA].pages[index].flags.raw |= VM_KRW;
        }
        mm_zone[MM_ZONE_DMA].free_pages -= NPAGE(size);
    }

    addr = PGROUND(VMA_LOW(_kernel_start));
    size = PGROUNDUP((_kernel_end - _kernel_start));
    zone = get_mmzone(addr, size);
    mm_zone_assert(zone);

    index = (addr - zone->start) / PAGESZ;
    for (int j = 0; j < (int)NPAGE(size); ++j, ++index)
    {
        atomic_write(&zone->pages[index].ref_count, 1);
        zone->pages[index].flags.mm_zone = zone - mm_zone;
        zone->pages[index].flags.raw |= VM_KRW;
    }

    zone->free_pages -= NPAGE(size);
    mm_zone_unlock(zone);
    printk("pages: %p\n", mm_zone[MM_ZONE_NORM].pages);
    return paging_init();
}

mm_zone_t *get_mmzone(uintptr_t addr, size_t size)
{
    if ((addr >= mm_zone[MM_ZONE_DMA].start) && ((addr + (size - 1)) < 0x1000000))
    {
        mm_zone_lock(&mm_zone[MM_ZONE_DMA]);
        if (mm_zone_isvalid(&mm_zone[MM_ZONE_DMA]))
            return &mm_zone[MM_ZONE_DMA];
        else return NULL;
    }
    if ((addr >= 0x1000000) && ((addr + (size - 1)) < (mm_zone[MM_ZONE_NORM].start + mm_zone[MM_ZONE_NORM].nrpages * PAGESZ)))
    {
        mm_zone_lock(&mm_zone[MM_ZONE_NORM]);
        if (mm_zone_isvalid(&mm_zone[MM_ZONE_NORM]))
            return &mm_zone[MM_ZONE_NORM];
        else return NULL;
    }
#ifdef ARCH_x86_64
    if ((addr >= 0x100000000) && ((addr + (size - 1)) < (mm_zone[MM_ZONE_HIGH].start + mm_zone[MM_ZONE_HIGH].nrpages * PAGESZ)))
    {
        mm_zone_lock(&mm_zone[MM_ZONE_HIGH]);
        if (mm_zone_isvalid(&mm_zone[MM_ZONE_HIGH]))
            return &mm_zone[MM_ZONE_HIGH];
        else return NULL;
    }
#endif
    return NULL;
}

mm_zone_t *mm_zone_get(int z)
{
    mm_zone_t *zone = NULL;
    if ((z < 0) || (z > NELEM(mm_zone)))
        return NULL;
    zone = &mm_zone[z];
    mm_zone_lock(zone);
    if (!mm_zone_isvalid(zone))
    {
        mm_zone_unlock(zone);
        return NULL;
    }
    return zone;
}