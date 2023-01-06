#include <printk.h>
#include <mm/shm.h>
#include <mm/pmm.h>
#include <mm/kalloc.h>
#include <sys/thread.h>
#include <lib/string.h>
#include <arch/i386/cpu.h>
#include <arch/i386/paging.h>
#include <bits/errno.h>
#include <lib/string.h>
#include <lime/assert.h>

void vmr_free(vmr_t *vmr)
{
    vmr_assert(vmr);
    vmr_lock(vmr);
    if (atomic_decr(&vmr->refs) > 1)
    {
        vmr_unlock(vmr);
        return;
    }
    
    if (vmr->lock)
        spinlock_free(vmr->lock);
    if (vmr->qnode)
    {
        queue_lock(vmr->qnode->queue);
        queue_remove_node(vmr->qnode->queue, vmr->qnode);
        queue_unlock(vmr->qnode->queue);
    }
    if (vmr->prev)
    {
        vmr_lock(vmr->prev);
        vmr->prev->next = vmr->next;
        vmr_unlock(vmr->prev);
    }
    if (vmr->next)
    {
        vmr_lock(vmr->next);
        vmr->next->prev = vmr->prev;
        vmr_unlock(vmr->next);
    }
    kfree(vmr);
}

int vmr_alloc(vmr_t **ref)
{
    int err = 0;
    vmr_t *vmr = NULL;
    spinlock_t *lock = NULL;
    assert(ref, "no reference pointer");
    if ((err = spinlock_init(NULL, "vmr", &lock)))
        goto error;
    if (!(vmr = kmalloc(sizeof *vmr)))
    {
        err = -ENOMEM;
        goto error;
    }
    memset(vmr, 0, sizeof *lock);
    vmr->lock = lock;
    atomic_incr(&vmr->refs);
    *ref = vmr;
    return 0;
error:
    if (vmr)
        kfree(vmr);
    if (lock)
        spinlock_free(lock);
    return err;
}