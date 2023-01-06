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
#include <sys/system.h>
#include <sys/proc.h>

int shm_alloc(shm_t **ref)
{
    int err = 0;
    shm_t *shm = NULL;
    uintptr_t pgdir = 0;
    spinlock_t *lock = NULL;
    queue_t *vmrs = NULL;
    spinlock_t *pgdirlk = NULL;
    assert(ref, "no shm reference");

    if ((err = spinlock_init(NULL, "shm", &lock)))
        goto error;

    if (!(pgdir = paging_getpgdir()))
    {
        err = -ENOMEM;
        goto error;
    }

    if ((err = spinlock_init(NULL, "shm-pgdir", &pgdirlk)))
        goto error;

    if ((err = queue_new("shm", &vmrs)))
        goto error;

    if (!(shm = kmalloc(sizeof *shm)))
    {
        err = -ENOMEM;
        goto error;
    }

    memset(shm, 0, sizeof *shm);
    shm->vmrs = vmrs;
    shm->lock = lock;
    shm->pgdir = pgdir;
    shm->pgdir_lock = pgdirlk;
    shm->stack_start = USER_STACK;
    shm->brk_size = USTACK_LIMIT;
    shm->available_size = USER_STACK;

    atomic_incr(&shm->refs);
    *ref = shm;
    return 0;
error:
    if (shm)
        kfree(shm);
    if (pgdirlk)
        spinlock_free(pgdirlk);
    if (pgdir)
        pmman.free(pgdir);
    if (lock)
        spinlock_free(lock);
    if (vmrs)
    {
        queue_lock(vmrs);
        queue_free(vmrs);
    };
    return err;
}

void shm_free(shm_t *shm)
{
    shm_assert(shm);
    shm_lock(shm);
    if (atomic_decr(&shm->refs) > 1)
    {
        shm_unlock(shm);
        return;
    }
    if (shm->lock)
        spinlock_free(shm->lock);
    if (shm->vmrs)
    {
        queue_lock(shm->vmrs);
        queue_free(shm->vmrs);
    }
    if (shm->pgdir)
        pmman.free(shm->pgdir);
    if (shm->pgdir_lock)
        spinlock_free(shm->pgdir_lock);
    kfree(shm);
}

static void vmr_linkto_left(shm_t *shm, vmr_t *vmr, vmr_t *left)
{
    assert(vmr, "no vmr");
    assert(left, "no left");
    vmr->next = left->next;
    if (left->next)
        left->next->prev = vmr;
    else
        shm->vmr_tail = vmr;
    left->next = vmr;
    vmr->prev = left;
}

static void vmr_linkto_right(shm_t *shm, vmr_t *vmr, vmr_t *right)
{
    assert(vmr, "no vmr");
    assert(right, "no right");
    vmr->prev = right->prev;
    if (right->prev)
        right->prev->next = vmr;
    else
        shm->vmr_head = vmr;
    right->prev = vmr;
    vmr->next = right;
}

int shm_map(shm_t *shm, vmr_t *vmr)
{
    vmr_t *tmp = NULL, *left = NULL, *right = NULL;
    shm_assert_lock(shm);
    vmr_lock(vmr);

    assert(vmr->vaddr, "invalid vmr");
    vmr->prev = NULL;
    vmr->next = NULL;

    queue_lock(shm->vmrs);
    if (!(vmr->qnode = enqueue(shm->vmrs, vmr)))
    {
        queue_unlock(shm->vmrs);
        vmr_unlock(vmr);
        return -ENOMEM;
    }
    queue_unlock(shm->vmrs);

    if (shm->vmr_head == 0)
    {
        shm->vmr_head = vmr;
        shm->vmr_tail = vmr;
        atomic_incr(&shm->nvmr);
        atomic_incr(&vmr->refs);
        vmr_unlock(vmr);
        return 0;
    }

    for (tmp = right = shm->vmr_head; tmp && (right->vaddr < vmr->vaddr); tmp = right = tmp->next)
        left = right;
    if (left)
        vmr_linkto_left(shm, vmr, left);
    if (right)
        vmr_linkto_right(shm, vmr, right);
    atomic_incr(&shm->nvmr);
    atomic_incr(&vmr->refs);
    // printk("left: %p, vmr: %p, right: %p\n", left, vmr, right);
    vmr_unlock(vmr);
    return 0;
}

vmr_t *shm_lookup(shm_t *shm, uintptr_t addr)
{
    vmr_t *tmp = NULL, *next = NULL;
    assert(addr, "invalid vmr");
    shm_assert_lock(shm);
    for (tmp = shm->vmr_head; tmp; tmp = next)
    {
        vmr_lock(tmp);
        if ((addr >= tmp->vaddr) && (addr < (tmp->vaddr + tmp->size)))
        {
            vmr_unlock(tmp);
            //printk("vmr: %p, oflags: 0%b, vflags: 0%b\n", tmp->vaddr, tmp->oflags, tmp->vflags);
            return tmp;
        }
        next = tmp->next;
        vmr_unlock(tmp);
    }
    return NULL;
}

int shm_contains(shm_t *shm, vmr_t *vmr)
{
    shm_assert_lock(shm);
    vmr_lock(vmr);
    queue_lock(shm->vmrs);
    int has = queue_contains_node(shm->vmrs, vmr->qnode);
    queue_unlock(shm->vmrs);
    vmr_unlock(vmr);
    return has;
}

int shm_remove(shm_t *shm, vmr_t *vmr)
{
    shm_assert_lock(shm);
    vmr_assert_lock(vmr);
    queue_lock(shm->vmrs);

    if (!queue_contains_node(shm->vmrs, vmr->qnode))
    {
        queue_unlock(shm->vmrs);
        return -ENOENT;
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
    if (shm->vmr_head == vmr)
        shm->vmr_head = vmr->next;
    if (shm->vmr_tail == vmr)
        shm->vmr_tail = vmr->prev;

    int ok = queue_remove_node(shm->vmrs, vmr->qnode);

    atomic_decr(&vmr->refs);
    queue_unlock(shm->vmrs);
    return ok;
}

int shm_unmap(shm_t *shm, vmr_t *vmr)
{
    int ret = 0;
    shm_assert_lock(shm);
    vmr_lock(vmr);
    ret = shm_remove(shm, vmr);
    vmr_unlock(vmr);
    return ret;
}

int shm_alloc_stack(shm_t *shm, vmr_t **ref)
{
    int err = 0;
    vmr_t *vmr = NULL;
    uintptr_t stack_addr = 0;

    if (ref == NULL)
        return -EINVAL;

    shm_assert_lock(shm);
    stack_addr = shm->stack_start - USTACKSIZE;

    if ((shm->available_size == 0) || (stack_addr <= USTACK_LIMIT))
        return -ENOMEM;

    if (shm_lookup(shm, stack_addr))
        return -ENOMEM;

    shm->stack_start -= USTACKSIZE;

    if ((err = vmr_alloc(&vmr)))
        return err;

    vmr->vflags = VM_URW;
    vmr->size = USTACKSIZE;
    vmr->vaddr = stack_addr;
    vmr->oflags = (VM_GROWSDOWN | VM_DONTEXPAND | VM_WRITE | VM_READ);

    if ((err = shm_map(shm, vmr)))
    {
        shm->stack_start += USTACKSIZE;
        goto error;
    }

    *ref = vmr;
    return 0;
error:
    if (vmr)
        vmr_free(vmr);
    return err;
}

int shm_copy(shm_t *dst, shm_t *src)
{
    int err =0;
    vmr_t *cpy = NULL, *next = NULL;
    assert(src, "no source memory map");
    assert(dst, "no destination memory map");
    shm_assert_lock(src);
    shm_assert_lock(dst);

    shm_assert_pgdir_lock(src);    
    shm_assert_pgdir_lock(dst);

    forlinked(vmr, src->vmr_head, next)
    {
        vmr_lock(vmr);
        if ((err = vmr_alloc(&cpy)))
        {
            vmr_unlock(vmr);
            goto error;
        }

        cpy->file = vmr->file;
        cpy->file_pos = vmr->file_pos;
        cpy->mmap_pos = vmr->mmap_pos;
        cpy->oflags = vmr->oflags;
        cpy->vflags = vmr->vflags;
        cpy->paddr = vmr->paddr;
        cpy->vaddr = vmr->vaddr;
        cpy->size = vmr->size;
        //printk("vmr: %p, size: %dB, vflags: %d\n", vmr->vaddr, vmr->size, vmr->vflags);
        next = vmr->next;
        vmr_unlock(vmr);
        if ((err = shm_map(dst, cpy)))
            goto error;
    }

    if ((err = paging_lazycopy(dst->pgdir, src->pgdir)))
        goto error;

    dst->stack_start = src->stack_start;
    dst->available_size = src->available_size;
    dst->arg_size = src->arg_size;
    dst->arg_start = src->arg_start;
    dst->brk_size = src->brk_size;
    dst->brk_start = src->brk_start;
    dst->code_size = src->code_size;
    dst->code_start = src->code_start;
    dst->data_size = src->data_size;
    dst->data_start = src->data_start;
    dst->env_size = src->env_size;
    dst->env_start = src->env_start;

    atomic_write(&dst->nmpgs, (atomic_t)atomic_read(&src->nmpgs));
    atomic_write(&dst->npgfaults, (atomic_t)atomic_read(&src->npgfaults));
    atomic_write(&dst->nvmr, (atomic_t)atomic_read(&src->nvmr));
    atomic_write(&dst->refs, (atomic_t)atomic_read(&src->refs));

    return 0;

error:
    klog(KLOG_WARN, "failed to copy mmap, need to reverse changes in dst mmap\n");
    return err;
}

int shm_unmap_addrspace(shm_t *shm)
{
    int err = 0;
    vmr_t *next = NULL;
    shm_assert_lock(shm);
    shm_assert_pgdir_lock(shm);

    forlinked(vmr, shm->vmr_head, next)
    {
        vmr_lock(vmr);
        next = vmr->next;

        if ((err = shm_remove(shm, vmr)))
        {
            vmr_unlock(vmr);
            goto error;
        }

        if ((err = paging_unmap_mapped(vmr->vaddr, GET_BOUNDARY_SIZE(vmr->vaddr, vmr->size))))
        {
            vmr_unlock(vmr);
            goto error;
        }
        
        vmr_unlock(vmr);
        vmr_free(vmr);
    }

    paging_proc_unmap(shm->pgdir);

    shm->stack_start = USER_STACK;
    
    shm->available_size = USER_STACK;
    
    shm->arg_size =0;
    shm->arg_start =0;
    
    shm->brk_size = USTACK_LIMIT;
    shm->brk_start = 0;
    
    shm->code_size =0;
    shm->code_start =0;
    
    shm->data_size =0;
    shm->data_start =0;
    
    shm->env_size =0;
    shm->env_start =0;
    atomic_write(&shm->nmpgs, 0);
    atomic_write(&shm->npgfaults, 0);
    atomic_write(&shm->nvmr, 0);
    atomic_write(&shm->refs, 1);
    shm->vmr_head = NULL;
    shm->vmr_tail = NULL;
    
    return 0;
error:
    return err;
}