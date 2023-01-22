#include <arch/i386/paging.h>
#include <bits/errno.h>
#include <lib/string.h>
#include <lime/assert.h>
#include <lime/string.h>
#include <mm/kalloc.h>
#include <printk.h>
#include <sys/sched.h>
#include <sys/thread.h>

int arch_thread_alloc(x86_thread_t **ref)
{
    int err =0;
    uintptr_t kstack = 0;
    x86_thread_t *tarch = NULL;
    assert(ref, "no reference pointer");

    if (!(tarch = kmalloc(sizeof *tarch)))
        return -ENOMEM;

    if (!(kstack = paging_alloc(KSTACKSIZE)))
    {
        err = -ENOMEM;
        goto error;
    }

    memset(tarch, 0, sizeof *tarch);
    tarch->kstack = kstack;
    *ref = tarch;

    return 0;
error:
    if (tarch)
        kfree(tarch);
    if (kstack)
        paging_free(kstack, KSTACKSIZE);
    printk("couldn't allocate tarch, error=%d\n", err);
    return err;
}

void arch_thread_free(x86_thread_t *tarch)
{
    assert(tarch, "no tarch to free");
    if (tarch->kstack)
        paging_free(tarch->kstack, KSTACKSIZE);
    kfree(tarch);
}

void arch_thread_exit(uintptr_t exit_code)
{
    //printk("TID(%d) exiting [%p]\n", current->t_tid, return_address(0));
    current_lock();
    current->t_state = T_ZOMBIE;
    current->t_exit = exit_code;
    sched();
    panic("thread: %d failed to zombie\n", current->t_tid);
    for (;;);
}

void thread_setkstack(x86_thread_t *thread)
{
    assert(thread, "no thread tarch");
    set_tss(thread->kstack + KSTACKSIZE, (SEG_KDATA << 3));
}