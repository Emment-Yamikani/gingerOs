#include <arch/i386/cpu.h>
#include <bits/errno.h>
#include <bits/dirent.h>
#include <fs/fs.h>
#include <mm/shm.h>
#include <lib/string.h>
#include <lime/string.h>
#include <lime/assert.h>
#include <printk.h>
#include <sys/mman/mman.h>
#include <sys/system.h>
#include <sys/thread.h>
#include <sys/proc.h>


void *mmap(void *addr __unused, size_t length __unused, int prot __unused, int flags __unused, int fd __unused, off_t offset __unused)
{
    return NULL;
}

int munmap(void *addr, size_t len)
{
    int err __unused = 0;
    shm_t *mmap = NULL;
    vmr_t *region = NULL;
    
    if ((((uintptr_t)addr) % PAGESZ) || (len % PAGESZ)) // ensure addr and len is page aligned
        return -EINVAL;

    if (addr == 0)
        return -EINVAL;

    current_lock();
    mmap = current->mmap;
    current_unlock();

    shm_lock(mmap);
    
    if ((region = shm_lookup(mmap, (uintptr_t)addr)) == NULL)
    {
        shm_unlock(mmap);
        return -EINVAL;
    }

    vmr_lock(region);

    vmr_unlock(region);

    shm_unlock(mmap);
    return 0;
}