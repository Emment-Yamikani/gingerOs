#include <bits/errno.h>
#include <sys/binfmt.h>
#include <mm/mmap.h>
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
} binfmt[] =
    {
        {.check = binfmt_elf_check, .load = binfmt_elf_load}};

int binfmt_load(const char *fn, proc_t *proc, proc_t **ref)
{
    int err = 0;
    uio_t uio = {0};
    int new = !proc;
    inode_t *image = NULL;
    uintptr_t oldpgdir = 0;

    assert(proc || ref, "no proc struct or ref");
    assert(fn, "path to program image not specified");

    current_assert();
    file_table_lock(current->t_file_table);
    uio = current->t_file_table->uio;
    file_table_unlock(current->t_file_table);

    if ((err = vfs_open(fn, &uio, O_RDONLY | O_EXEC, 0, &image)))
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
        proc_lock(proc);
        mmap_lock(proc->mmap);
        oldpgdir = arch_proc_init(proc->mmap);
    }
    else
    {
        proc_assert_lock(proc);
        if ((err = thread_kill_all()))
            goto error;

        atomic_write(&proc->tgroup->nthreads, 1);
        proc->tmain = current;
        mmap_assert_locked(proc->mmap);
    }

    for (int i = 0; i < NELEM(binfmt); ++i)
    {
        if ((err = binfmt[i].check(image)))
        {
            paging_switch(oldpgdir);
            if (new){
                mmap_unlock(proc->mmap);
                proc_unlock(proc);
            }
            goto error;
        }

        if ((err = binfmt[i].load(image, proc)))
        {
            paging_switch(oldpgdir);
            if (new){
                mmap_unlock(proc->mmap);
                proc_unlock(proc);
            }
            goto error;
        }
    }

    paging_switch(oldpgdir);

    if (new){
        mmap_unlock(proc->mmap);
        proc_unlock(proc);
    }

    // printk("BINELF: brk_start: %p\n", proc->mmap->brk_start);
    if (ref)
        *ref = proc;

    return 0;
error:
    klog(KLOG_FAIL, "failed to load program image \'%s\', error=%d\n", fn, err);
    return err;
}