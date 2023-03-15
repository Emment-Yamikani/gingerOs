#include <arch/sys/proc.h>
#include <arch/i386/paging.h>
#include <arch/system.h>

uintptr_t arch_proc_init(mmap_t *mmap)
{
    mmap_assert_locked(mmap);
    uintptr_t cr3 = PGROUND(read_cr3());
    paging_switch(mmap->pgdir);
    return cr3;
}