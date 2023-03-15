#include <fs/fs.h>
#include <arch/i386/cpu.h>
#include <sys/proc.h>
#include <sys/thread.h>
#include <printk.h>
#include <mm/mmap.h>
#include <sys/mman/mman.h>
#include <bits/errno.h>
#include <sys/sysproc.h>
#include <arch/i386/paging.h>

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
    int err = 0;
    mmap_t *mm = NULL;
    file_t *file = NULL;
    vmr_t *region = NULL;
    file_table_t *table = NULL;

    printk("[%d:%d:%d]: mmap(%p, %d, %d, %d, %d, %d)\n", thread_self(), getpid(), getppid(), addr, len, prot, flags, fd, off);

    if (__flags_anon(flags) && fd != -1)
        return (void*)-EINVAL;

    mmap_lock((mm = proc->mmap));

    if ((err = mmap_map_region(mm, (uintptr_t)addr, len, prot, flags, &region)))
        goto error;
    addr = (void *)region->start;

    if (__flags_anon(flags))
        goto anon;

    file_table_lock((table = proc->ftable));
    
    if ((err = check_fildes(fd, table)))
        goto error;

    if ((file = fileget(table, fd)) == NULL)
    {
        err = -EBADF;
        goto error;
    }

    region->file_pos = off;
    region->filesz = len;

    if ((err = fmmap(file, region)))
        goto error;

    goto done;

anon:
    if (__flags_mapin(flags)) {
        if ((err = paging_mappages(PGROUND(addr), GET_BOUNDARY_SIZE(0, __vmr_size(region)), region->vflags)))
            goto error;
        if (__flags_zero(flags))
            memset((void *)PGROUND(addr), 0, GET_BOUNDARY_SIZE(0, __vmr_size(region)));
    }
done:

    if (table && spin_holding(table->lock))
        file_table_unlock(table);

    mmap_unlock(mm);
    return addr;

error:
    if (table && spin_holding(table->lock))
        file_table_unlock(table);

    if (mm && region)
        mmap_remove(mm, region);
    
    if (mm)
        mmap_dump_list(*mm);

    if (mm && mmap_holding(mm))
        mmap_unlock(mm);
    

    printk("err: %d\n", err);
    return (void *)err;
}

int munmap(void *addr, size_t length);