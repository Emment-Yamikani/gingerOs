#include <bits/errno.h>
#include <sys/binfmt.h>
#include <mm/shm.h>
#include <lime/assert.h>
#include <printk.h>
#include <lib/string.h>
#include <arch/i386/paging.h>
#include <sys/thread.h>
#include <arch/sys/proc.h>
#include <sys/elf/elf.h>

struct binfmt
{
    int (*check)(inode_t *);
    int (*load)(inode_t *, proc_t *);
}binfmt[] =
{
    {.check = binfmt_elf_check, .load = binfmt_elf_load}
};

int binfmt_load(const char *fn, proc_t *proc, proc_t **ref)
{
    uio_t uio;
    int err = 0;
    int new = !proc;
    inode_t *image = NULL;
    uintptr_t oldpgdir = 0;

    assert(proc || ref, "no proc struct or ref");
    assert(fn, "path to program image not specified");

    current_assert();
    file_table_lock(current->t_file_table);
    uio = current->t_file_table->uio;
    file_table_unlock(current->t_file_table);

    if ((err = vfs_open(fn, &uio, O_RDONLY | O_EXEC, &image)))
        goto error;

    switch (image->i_type)
    {
    case FS_BLKDEV:
        err = -ENOEXEC;
        goto error;
    case FS_CHRDEV:
        err = -ENOEXEC;
        goto error;
    case FS_PIPE:
        err = -ENOEXEC;
        goto error;
    case FS_INV:
        err = -ENOEXEC;
        goto error;
    case FS_DIR:
        err = -EISDIR;
        goto error;
    case FS_RGL:
        break;
    }

    if (new)
    {
        if ((err = proc_alloc(fn, &proc)))
            goto error;
        shm_lock(proc->mmap);
        oldpgdir = arch_proc_init(proc->mmap);
    }
    else
    {
        if ((err = thread_kill_all()))
            goto error;
        
        atomic_write(&proc->tgroup->nthreads, 1);
        proc->tmain = current;

        shm_lock(proc->mmap);
        shm_pgdir_lock(proc->mmap);
        err = shm_unmap_addrspace(proc->mmap);
        if (err && err != -ENOENT)
        {
            printk("failed to unmap addrspace\n");
            shm_pgdir_unlock(proc->mmap);
            shm_unlock(proc->mmap);
            goto error;
        }
        shm_pgdir_unlock(proc->mmap);
    }


    for (int i =0; i < NELEM(binfmt); ++i)
    {
        if ((err = binfmt[i].check(image))){
            paging_switch(oldpgdir);
            shm_unlock(proc->mmap);
            goto error;
        }
        
        if ((err = binfmt[i].load(image, proc))){
            paging_switch(oldpgdir);
            shm_unlock(proc->mmap);
            goto error;
        }
    }

    paging_switch(oldpgdir);
    shm_unlock(proc->mmap);

    //printk("BINELF: brk_start: %p\n", proc->mmap->brk_start);
    if (ref) *ref = proc;

    return 0;
error:
    klog(KLOG_FAIL, "failed to load program image \'%s\', error=%d\n", fn, err);
    return err;
}