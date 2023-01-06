#include <arch/sys/proc.h>
#include <arch/i386/paging.h>
#include <arch/system.h>

uintptr_t arch_proc_init(shm_t *shm)
{
    shm_assert_lock(shm);
    uintptr_t cr3 = PGROUND(read_cr3());
    paging_switch(shm->pgdir);
    return cr3;
}