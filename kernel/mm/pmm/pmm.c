#include <arch/boot/boot.h>
#include <ds/bitmap.h>
#include <mm/pmm.h>
#include <printk.h>
#include <lib/string.h>
#include <arch/i386/32bit.h>
#include <sys/system.h>
#include <arch/i386/paging.h>

extern uintptr_t _kernel_start;
extern uintptr_t _kernel_end;

struct pmman pmman;
static uint32_t map[MAX_INT_BITMAP(128)] = {0};
static bitmap_t *bitmap = BITMAP_NEW("pmmbitmap", map);

struct pm_frame_ frames[MAXFRAMES] = {0};
spinlock_t *frameslk = SPINLOCK_NEW("frameslk");


struct
{
    atomic_t maxblocks;
    atomic_t usedblocks;
} pmem;



long frames_incr(int frame)
{
    assert((frame < MAXFRAMES) && (frame >=0), "frame out of bounds");
    assert(!spin_try_lock(frameslk), "caller must hold frameslk");
    return frames[frame].refs++;
}

long frames_decr(int frame)
{
    assert((frame < MAXFRAMES) && (frame >= 0), "frame out of bounds");
    assert(!spin_try_lock(frameslk), "caller must hold frameslk");
    return frames[frame].refs--;
}

long frames_get_refs(int frame)
{
    assert((frame < MAXFRAMES) && (frame >= 0), "frame out of bounds");
    assert(!spin_try_lock(frameslk), "caller must hold frameslk");
    return frames[frame].refs;
}

int pmm_init(void)
{
    int ret = 0;
    size_t size = 0;
    uintptr_t addr = 0;

    bitmap_init(bitmap, -1);
    bitmap->size = (bootinfo.pmemsize / 32);
    memsetd(frames, 1, (sizeof frames) / 4);

    for (int i = 0; i < bootinfo.mmap_count; ++i)
    {
        if ((bootinfo.mmap[i].addr == 0x100000) && (bootinfo.mmap[i].type == MULTIBOOT_MEMORY_AVAILABLE))
        {
            addr = 0x1000000;
            size = bootinfo.mmap[i].size - addr;
            bitmap_lock(bitmap);
            frames_lock();
            bitmap_unset(bitmap, addr / PAGESZ, (size / PAGESZ));

            for (int bit = (addr / PAGESZ); bit < (int)(size / PAGESZ); ++bit)
                frames_decr(bit);

            atomic_write(&pmem.maxblocks, bootinfo.pmemsize / 4);
            atomic_write(&pmem.usedblocks, addr / PAGESZ);

            frames_unlock();
            bitmap_unlock(bitmap);
        }

        if (bootinfo.mmap[i].type == MULTIBOOT_MEMORY_ACPI_RECLAIMABLE)
        {
            addr = bootinfo.mmap[i].addr;
            if ((ret = paging_identity_map(addr, addr, bootinfo.mmap[i].size, VM_KRW | VM_PCD | VM_PWT)))
                return ret;
        }
        else if ((bootinfo.mmap[i].addr > 0x100000) && (bootinfo.mmap[i].addr < MMAP_DEVADDR))
        {
            addr = bootinfo.mmap[i].addr;
            if ((ret = paging_identity_map(addr, addr, bootinfo.mmap[i].size, VM_KRW | VM_PCD | VM_PWT)))
                return ret;
        }
    }

    return paging_init();
}

uintptr_t pmm_alloc(void)
{
    int bit = 0;
    uintptr_t addr = 0;
    bitmap_lock(bitmap);
    frames_lock();
    if ((bit = bitmap_first_unset(bitmap, 0, 1)) < 0)
    {
        frames_unlock();
        bitmap_unlock(bitmap);
        return 0;
    }
    bitmap_set(bitmap, bit, 1);
    frames_incr(bit);

    atomic_incr(&pmem.usedblocks);

    frames_unlock();
    bitmap_unlock(bitmap);
    addr = bit * PAGESZ;
    return addr;
}

void pmm_free(uintptr_t addr)
{
    int ret =0;
    int bit = addr / PAGESZ;

    if (addr < 0x100000)
        return;

    bitmap_lock(bitmap);
    frames_lock();
    
    if ((ret = frames_decr(bit)) <= 0)
    {
        panic("phyaddr: %p was free before\n", addr);
    }
    else if (ret > 1)
    {
        frames_unlock();
        bitmap_unlock(bitmap);
        return;
    }

    atomic_decr(&pmem.usedblocks);
    bitmap_unset(bitmap, bit, 1);

    frames_unlock();
    bitmap_unlock(bitmap);
}

size_t pmm_mem_used(void)
{
    bitmap_lock(bitmap);
    size_t size = (atomic_read(&pmem.usedblocks) * PAGESZ) / 1024;
    bitmap_unlock(bitmap);
    return size;
}

size_t pmm_mem_free(void)
{
    bitmap_lock(bitmap);
    size_t size = ((atomic_read(&pmem.maxblocks) - atomic_read(&pmem.usedblocks)) * PAGESZ) / 1024;
    bitmap_unlock(bitmap);
    return size;
}

struct pmman pmman = 
{
    .init = pmm_init,
    .alloc = pmm_alloc,
    .free = pmm_free,
    .mem_free = pmm_mem_free,
    .mem_used = pmm_mem_used
};