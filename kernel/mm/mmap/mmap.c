#include <mm/kalloc.h>
#include <mm/mmap.h>
#include <mm/pmm.h>
#include <arch/i386/paging.h>

#include <printk.h>

void vmr_dump(vmr_t *r, int i)
{
    printk("memory %d: [0x%08lX : 0x%08lX] %8ld [%5s] %s%s%s%s [%s-%s] refs: %d|\n", i++,
           r->start, r->end, __vmr_size(r) / 1024,
           __isstack(r) ? "stack" : __vmr_exec(r) ? ".text"
                                : __vmr_rw(r)     ? ".data"
                                : __vmr_read(r)   ? "rodat"
                                                  : "inval",
           __vmr_read(r) ? "r" : "_",
           __vmr_write(r) ? "w" : "_", __vmr_exec(r) ? "x" : "_",
           __vmr_shared(r) ? "S" : "P", __vmr_dontexpand(r) ? " " : __vmr_growsup(r) ? "U" : "D",
           __vmr_growsdown(r) ? "d" : "u", r->refs);
}

void mmap_dump_list(mmap_t mmap)
{
    int i = 0, j = 0;
    size_t holesz = 0;
    printk("\n_______________________________________________________________________\n");
    printk("\tMemory Map[0x%08lX : 0x%08lX] Used: %ldKiB refs: %d\n",
           (mmap.vmr_head ? mmap.vmr_head->start : 0),
           (mmap.vmr_tail ? mmap.vmr_tail->end : 0), mmap.used_space / 1024, mmap.refs);
    
    printk("\n%-8s %18s %17s %5s %5s %4s \n", "Mapping", "Range", "Size(KiB)", "Type", "Mode", "Exp");
    printk("_________ _________________________ ________ _______ ____ _____ _______\n");
    
    if (mmap.vmr_head == NULL) {
        mmap_holesize(&mmap, 0, &holesz);
        if (holesz) {
            printk("hole   %d: [0x%08lX : 0x%08lX] %8ld [%5s]                   |\n",
                   j++, (uintptr_t)0, holesz - 1, holesz / 1024, "free");
        }

        printk("_______________________________________________________________________/\n");
        return;
    }
    
    holesz = mmap.vmr_head->start;
    if (holesz)
        printk("hole   %d: [0x%08lX : 0x%08lX] %8ld [%5s]                   |\n", j++,
                (uintptr_t)0, holesz - 1, holesz / 1024, "free");

    forlinked(r, mmap.vmr_head, r->next)
    {
        vmr_dump(r, i++);
        if (r->next) {
            holesz = __vmr_next(r)->start - __vmr_upper_bound(r);
            if (holesz)
                printk("hole   %d: [0x%08lX : 0x%08lX] %8ld [%5s]                   |\n", j++,
                    __vmr_upper_bound(r), __vmr_lower_bound(r->next), holesz / 1024, "free");
            continue;
        }
        mmap_holesize(&mmap, __vmr_upper_bound(r), &holesz);
        if (holesz)
        {
            printk("hole   %d: [0x%08lX : 0x%08lX] %8ld [%5s]                   |\n", j++,
                __vmr_upper_bound(r), __vmr_upper_bound(r) + holesz - 1, holesz / 1024, "free");
        }
    }

    printk("_______________________________________________________________________/\n");

}

int mmap_init(mmap_t *mmap)
{
    int err = 0;

    if (mmap == NULL)
        return -EINVAL;

    mmap_assert_locked(mmap);

    if ((err = mmap_clean(mmap)))
        return err;

    if ((err = mmap_map_region(mmap, 0, 0x1000000, 0, MAP_FIXED | MAP_PRIVATE | MAP_DONTEXPAND, NULL)))
        return err;

    return 0;
}

int mmap_alloc(mmap_t **ref)
{
    int err = 0;
    uintptr_t pgdir = 0;
    mmap_t *mmap = NULL;
    spinlock_t *lock = NULL;

    if ((err = spinlock_init(NULL, "mmap", &lock)))
        return err;

    if ((pgdir = paging_getpgdir()) == 0)
    {
        spinlock_free(lock);
        return -ENOMEM;
    }
    
    if ((mmap = kmalloc(sizeof *mmap)) == NULL) {
        pmman.free(pgdir);
        spinlock_free(lock);
        return -ENOMEM;
    }

    memset(mmap, 0, sizeof *mmap);
    
    mmap->lock = lock;
    mmap->pgdir = pgdir;
    mmap->flags = MMAP_USER;
    mmap->guard_len = PAGESZ;
    mmap->limit = __mmap_limit;

    mmap_lock(mmap);
    err = mmap_init(mmap);
    mmap_unlock(mmap);
    
    *ref = mmap;
    return 0;
}

int mmap_mapin(mmap_t *mmap, vmr_t *r)
{
    size_t holesz = 0;
    uintptr_t addr = 0;
    vmr_t *left = NULL, *right = NULL;

    if (mmap == NULL || r == NULL)
        return -EINVAL;

    mmap_assert_locked(mmap);

    if (__vmr_size(r) == 0)
        return -EINVAL;

    if (r->end > __mmap_limit)
        return -EINVAL;

    if (mmap_contains(mmap, r))
        return -EEXIST;

    addr = r->start;
    while (addr <= r->end)
    {
        if (mmap_find(mmap, addr))
            return -EEXIST;
        mmap_holesize(mmap, addr, &holesz);
        addr += holesz;
        if (holesz == 0)
            printk("%s:%d: endless loop\n", __FILE__, __LINE__);
    }

    r->next = NULL;
    r->prev = NULL;

    if (mmap->vmr_head == NULL)
    {
        mmap->vmr_head = mmap->vmr_tail = r;
        r->refs++;
        r->refs++;
        goto done;
    }

    right = mmap->vmr_head;
    for (; right && right->start < r->start;)
    {
        if (__vmr_upper_bound(right) <= r->start)
            left = right;
        right = right->next;
    }

    if (left && left->start > r->start)
    {
        left = NULL;
    }

    if (left)
    {
        r->prev = left;
        
        if (left->next == NULL)
            left->refs++;
        
        r->next = left->next;

        if (left->next)
        {
            //left->next->refs++;
            left->next->prev = r;
            r->refs++;
        }
        else
        {
            mmap->vmr_tail->refs--;
            mmap->vmr_tail = r;
            r->refs++;
        }
        
        left->next = r;
        r->refs++;
    }
    else if (right)
    {
        r->next = right;
        
        if (right->prev == NULL)
            right->refs++;
        
        r->prev = right->prev;

        if (right->prev)
        {
            //right->prev->refs++;
            right->prev->next = r;
            r->refs++;
        }
        else
        {
            mmap->vmr_head->refs--;
            mmap->vmr_head = r;
            r->refs++;
        }
        
        right->prev = r;
        r->refs++;
    }

done:
    r->mmap = mmap;
    mmap->refs++;
    mmap->used_space += __vmr_size(r);
    return 0;
}

int mmap_forced_mapin(mmap_t *mmap, vmr_t *r)
{
    int err = 0;
    if (mmap == NULL || r == NULL)
        return -EINVAL;
    
    mmap_assert_locked(mmap);

    if ((err = mmap_unmap(mmap, r->start, __vmr_size(r))))
        return err;
    return mmap_mapin(mmap, r);
}

int mmap_contains(mmap_t *mmap, vmr_t *r)
{
    if (mmap == NULL || r == NULL)
        return -EINVAL;

    mmap_assert_locked(mmap);

    forlinked(tmp, mmap->vmr_head, tmp->next)
    {
        if (tmp == r)
            return 1;
    }
    return 0;
}

int mmap_remove(mmap_t *mmap, vmr_t *r)
{
    if (mmap == NULL || r == NULL)
        return -EINVAL;

    mmap_assert_locked(mmap);

    if (!mmap_contains(mmap, r))
        return -ENOENT;

    if (r->prev)
    {
        r->prev->next = r->next;
        r->refs--;
    }

    if (r->next)
    {
        r->next->prev = r->prev;
        r->refs--;
    }

    if (mmap->vmr_head == r)
    {
        mmap->vmr_head = r->next;
        if (mmap->vmr_head){
            mmap->vmr_head->prev = NULL;
            if (mmap->vmr_head->next == NULL)
                mmap->vmr_head->refs--;
        }
    }

    if (mmap->vmr_tail == r)
    {
        mmap->vmr_tail = r->prev;
        if (mmap->vmr_tail){
            mmap->vmr_tail->next = NULL;
            if (mmap->vmr_tail->prev == NULL)
                mmap->vmr_tail->refs--;
        }
    }

    r->refs--;
    r->mmap = NULL;
    mmap->refs--;

    paging_unmap_mapped(r->start, __vmr_size(r));
    vmr_free(r);
    return 0;
}

vmr_t *mmap_find(mmap_t *mmap, uintptr_t addr)
{
    if (mmap == NULL)
        return NULL;

    mmap_assert_locked(mmap);

    forlinked(tmp, mmap->vmr_head, tmp->next)
    {
        if (addr >= tmp->start && addr <= tmp->end)
            return tmp;
    }

    return NULL;
}

vmr_t *mmap_find_exact(mmap_t *mmap, uintptr_t start, uintptr_t end)
{
    vmr_t *vmr = NULL;
    if (mmap == NULL)
        return NULL;

    mmap_assert_locked(mmap);

    vmr = mmap_find(mmap, start);
    if (vmr == NULL || start != vmr->start || end != vmr->end)
        return NULL;
    return vmr;
}

vmr_t *mmap_find_vmr_next(mmap_t *mmap, uintptr_t addr, vmr_t **pnext)
{
    vmr_t *r = NULL;
    if (mmap == NULL || pnext == NULL)
        return NULL;

    mmap_assert_locked(mmap);

    *pnext = NULL;

    if ((r = mmap_find(mmap, addr)))
    {
        *pnext = r->next;
        return r;
    }

    forlinked(tmp, mmap->vmr_head, tmp->next)
    {
        if (addr < tmp->start)
        {
            *pnext = tmp;
            return NULL;
        }
    }
    return NULL;
}

vmr_t *mmap_find_vmr_prev(mmap_t *mmap, uintptr_t addr, vmr_t **pprev)
{
    vmr_t *r = NULL;
    if (mmap == NULL || pprev == NULL)
        return NULL;

    mmap_assert_locked(mmap);

    *pprev = NULL;

    if ((r = mmap_find(mmap, addr)))
    {
        *pprev = r->prev;
        return r;
    }

    forlinked(tmp, mmap->vmr_tail, tmp->prev)
    {
        if (tmp->end < addr)
        {
            *pprev = tmp;
            return NULL;
        }
    }
    return NULL;
}

vmr_t *mmap_find_vmr_overlap(mmap_t *mmap, uintptr_t start, uintptr_t end)
{
    vmr_t *r = NULL;
    if (mmap == NULL)
        return NULL;

    mmap_assert_locked(mmap);

    r = mmap_find(mmap, start);
    if (r && end <= r->end)
        return r;
    return NULL;
}

int mmap_holesize(mmap_t *mmap, uintptr_t addr, size_t *plen)
{
    vmr_t *next = NULL;
    if (mmap == NULL || plen == NULL)
        return -EINVAL;

    mmap_assert_locked(mmap);

    *plen = 0;

    if (addr > mmap->limit)
        return -EINVAL;

    if (!__ishole(mmap, addr))
        return -EINVAL;

    mmap_find_vmr_next(mmap, addr, &next);

    if (next == NULL)
        *plen = (mmap->limit + 1) - addr;
    else
        *plen = next->start - addr;
    return 0;
}

int mmap_unmap(mmap_t *mmap, uintptr_t start, size_t len)
{
    vmr_t *vmr = NULL;
    size_t holesz = 0;
    uintptr_t end = start + len - 1;

    if (mmap == NULL || len == 0)
        return -EINVAL;

    mmap_assert_locked(mmap);

    if (!__valid_addr(end))
        return -EINVAL;

    while (len)
    {
        end = start + len - 1;
        if ((vmr = mmap_find_exact(mmap, start, end)))
        {
            len -= end - start;
            start += end - start;
            mmap_remove(mmap, vmr);
        }
        else if ((vmr = mmap_find(mmap, start)))
        {
            if (vmr->start == start)
            {
                if (__vmr_size(vmr) > len)
                {
                    vmr->start += len;
                    len = 0;
                }
                else if (__vmr_size(vmr) < len)
                {
                    start = __vmr_upper_bound(vmr);
                    len -= __vmr_size(vmr);
                    mmap_remove(mmap, vmr);
                }
            }
            else if (vmr->end == start)
            {
                vmr->end -= 1;
            }
            else
                vmr_split(vmr, start, NULL);
        }
        else
        {
            mmap_holesize(mmap, start, &holesz);
            if (holesz == 0)
            {
                printk("[%lX] Is Not a valid hole\n", start);
                break;
            }
            holesz = holesz > len ? len : holesz;
            start += holesz;
            len -= holesz;
        }
    }

    return 0;
}

int mmap_find_hole(mmap_t *mmap, size_t size, uintptr_t *paddr, int whence)
{
    size_t holesz = 0;

    if (mmap == NULL || paddr == NULL || size == 0)
        return -EINVAL;

    mmap_assert_locked(mmap);

    *paddr = 0;

    if (whence == __whence_start)
    {
        mmap_holesize(mmap, 0, &holesz);
        if (holesz >= size)
        {
            *paddr = 0;
            return 0;
        }

        forlinked(tmp, mmap->vmr_head, __vmr_next(tmp))
        {
            if (__vmr_next(tmp))
            {
                holesz = __vmr_next(tmp)->start - __vmr_upper_bound(tmp);
                if (holesz >= size)
                {
                    *paddr = __vmr_upper_bound(tmp);
                    return 0;
                }
                continue;
            }
            mmap_holesize(mmap, __vmr_upper_bound(tmp), &holesz);
            if (holesz >= size)
            {
                *paddr = __vmr_upper_bound(tmp);
                return 0;
            }
            goto error;
        }
    }
    else if (whence == __whence_end)
    {
        mmap_holesize(mmap, (mmap->limit + 1) - size, &holesz);
        if (holesz >= size)
        {
            *paddr = (mmap->limit + 1) - size;
            return 0;
        }

        forlinked(tmp, mmap->vmr_tail, __vmr_prev(tmp))
        {
            if (__vmr_prev(tmp))
            {
                holesz = tmp->start - __vmr_upper_bound(__vmr_prev(tmp));
                if (holesz >= size)
                {
                    *paddr = tmp->start - size;
                    return 0;
                }
                continue;
            }

            if (__vmr_lower_bound(tmp) > (uintptr_t)NULL)
            {
                holesz = tmp->start;
                if (holesz >= size)
                {
                    *paddr = tmp->start - size;
                    return 0;
                }
            }
        }
    }

error:
    return -ENOMEM;
}

int mmap_find_holeat(mmap_t *mmap, uintptr_t addr, size_t size, uintptr_t *paddr, int whence)
{
    size_t holesz = 0;
    vmr_t *near_vmr = NULL;

    if (mmap == NULL || paddr == NULL || size == 0)
        return -EINVAL;

    mmap_assert_locked(mmap);

    if (addr)
    {
        if (whence == __whence_start)
            mmap_find_vmr_next(mmap, addr, &near_vmr);
        else if (whence == __whence_end)
            mmap_find_vmr_prev(mmap, addr, &near_vmr);
    }

    *paddr = 0;

    if (whence == __whence_start)
    {

        mmap_holesize(mmap, addr, &holesz);
        if (holesz >= size)
        {
            *paddr = addr;
            return 0;
        }

        near_vmr = near_vmr ? near_vmr : mmap->vmr_head;

        if (near_vmr && near_vmr->prev)
        {
            holesz = near_vmr->start - __vmr_upper_bound(__vmr_prev(near_vmr));
            if (holesz >= size)
            {
                *paddr = __vmr_upper_bound(__vmr_prev(near_vmr));
                return 0;
            }
        }

        forlinked(tmp, near_vmr, __vmr_next(tmp))
        {
            if (__vmr_next(tmp))
            {
                holesz = __vmr_next(tmp)->start - __vmr_upper_bound(tmp);
                if (holesz >= size)
                {
                    *paddr = __vmr_upper_bound(tmp);
                    return 0;
                }
                continue;
            }
            mmap_holesize(mmap, __vmr_upper_bound(tmp), &holesz);
            if (holesz >= size)
            {
                *paddr = __vmr_upper_bound(tmp);
                return 0;
            }
            goto error;
        }
    }
    else if (whence == __whence_end)
    {
        near_vmr = near_vmr ? near_vmr : mmap->vmr_tail;

        if (near_vmr)
            goto find1;

        mmap_holesize(mmap, (mmap->limit + 1) - size, &holesz);
        if (holesz >= size)
        {
            *paddr = (mmap->limit + 1) - size;
            return 0;
        }

    find1:
        if (near_vmr)
        {
            if (near_vmr->next)
            {
                holesz = __vmr_next(near_vmr)->start - __vmr_upper_bound(near_vmr);
                if (holesz >= size)
                {
                    *paddr = __vmr_next(near_vmr)->start - size;
                    return 0;
                }
            }
            else
            {
                mmap_holesize(mmap, __vmr_upper_bound(near_vmr), &holesz);
                if (holesz > size)
                {
                    *paddr = __vmr_upper_bound(near_vmr) + holesz - size;
                    return 0;
                }
                else if (holesz == size)
                {
                    *paddr = __vmr_upper_bound(near_vmr);
                    return 0;
                }
            }
        }

        forlinked(tmp, near_vmr, __vmr_prev(tmp))
        {
            if (__vmr_prev(tmp))
            {
                holesz = tmp->start - __vmr_upper_bound(__vmr_prev(tmp));
                if (holesz >= size)
                {
                    *paddr = tmp->start - size;
                    return 0;
                }
                continue;
            }

            if (__vmr_lower_bound(tmp) > (uintptr_t)NULL)
            {
                holesz = tmp->start;
                if (holesz >= size)
                {
                    *paddr = tmp->start - size;
                    return 0;
                }
            }
        }
    }

error:
    return mmap_find_hole(mmap, size, paddr, whence);
}

int mmap_map_region(mmap_t *mmap, uintptr_t addr, size_t len, int prot, int flags, vmr_t **pvmr)
{
    int err = 0;
    int whence = 0;
    vmr_t *r = NULL;

    if (mmap == NULL || (!__flags_fixed(flags) && pvmr == NULL) || len == 0)
        return -EINVAL;

    mmap_assert_locked(mmap);

    if (pvmr) *pvmr = NULL;

    /*If MAP_FIXED is set ensure 'addr' is Page aligned.*/
    if (__flags_fixed(flags) && !__isaligned(addr))
        return -EINVAL;

    if (__flags_fixed(flags) && !__valid_addr(addr))
        return -EINVAL;

    /*If MAP_STACK is specified ensure 'addr' and 'len' are Page aligned.*/
    if (__flags_stack(flags) && (!__isaligned(addr) || !__isaligned(len)))
        return -EINVAL;

    /*If both MAP_PRIVATE and MAP_SHARED are specified,
     *or none are specified return -EINVAL*/
    if ((__flags_private(flags) && __flags_shared(flags)) ||
        (!__flags_private(flags) && !__flags_shared(flags)))
        return -EINVAL;

    /*A stack must always be read and write at the most*/
    if (__flags_stack(flags) && !__prot_rw(prot))
        return -EINVAL;

    if (__prot_write(prot) && __prot_exec(prot))
        return -EACCES;

    len = __flags_stack(flags) ? len + mmap->guard_len : len;
    whence = __flags_stack(flags) ? __whence_end : __whence_start;

    if (!__flags_fixed(flags))
    {
        if ((err = mmap_find_holeat(mmap, addr, len, &addr, whence)))
            return err;
    }

    if ((err = vmr_alloc(&r)))
        return err;

    r->start = addr;
    r->end = addr + len - 1;

    r->flags |= __prot_exec(prot) ? VM_EXEC : 0;
    r->flags |= __prot_read(prot) ? VM_READ : 0;
    r->flags |= __prot_write(prot) ? VM_WRITE : 0;
    
    r->flags |= __flags_zero(flags) ? VM_ZERO : 0;
    r->flags |= __flags_shared(flags) ? VM_SHARED : 0;
    r->flags |= __flags_stack(flags) ? VM_GROWSDOWN : 0;
    r->flags |= __flags_dontexpand(flags) ? VM_DONTEXPAND : 0;

    r->vflags |= __vmr_read(r) ? VM_R : 0;
    r->vflags |= __vmr_write(r) ? VM_W : 0;
    r->vflags |= __vmr_exec(r) ? VM_X : 0;
    r->vflags |= mmap->flags & MMAP_USER ? VM_U : 0;

    if (__flags_fixed(flags))
    {
        if ((err = mmap_forced_mapin(mmap, r)))
        {
            vmr_free(r);
            return err;
        }
    } else if ((err = mmap_mapin(mmap, r)))
    {
        vmr_free(r);
        return err;
    }

    if (pvmr) *pvmr = r;
    return 0;
}

int mmap_alloc_vmr(mmap_t *mmap, size_t size, int prot, int flags, vmr_t **pvmr)
{
    if (mmap == NULL || pvmr == NULL)
        return -EINVAL;
    mmap_assert_locked(mmap);
    if (__flags_fixed(flags))
        return -EINVAL;
    *pvmr = NULL;
    return mmap_map_region(mmap, 0, size, prot, flags, pvmr);
}

int mmap_alloc_stack(mmap_t *mmap, size_t size, vmr_t **pstack)
{
    if (mmap == NULL || pstack == NULL)
        return -EINVAL;

    mmap_assert_locked(mmap);

    *pstack = NULL;
    return mmap_map_region(mmap, 0, size, PROT_READ | PROT_WRITE, MAP_STACK | MAP_PRIVATE, pstack);
}

int mmap_vmr_expand(mmap_t *mmap, vmr_t *r, intptr_t incr)
{
    int err = 0;
    size_t holesz = 0;
    uintptr_t hole_addr = 0;
    long oldsz = 0, newsz = 0;

    if (mmap == NULL || r == NULL || incr == 0)
        return -EINVAL;

    mmap_assert_locked(mmap);

    /**Do not acknowledge request if the region is marked DONTEXPAND
     * Regardless of the expansion type.
     */
    if (__vmr_dontexpand(r))
        return -EINVAL;

    /*Get expansion size*/
    newsz = (oldsz = __vmr_size(r)) + incr;

    /*Region grows UP*/
    if (__vmr_growsup(r))
    {
        if (newsz > oldsz)
        {
            hole_addr = __vmr_upper_bound(r);
            if ((err = mmap_holesize(mmap, hole_addr, &holesz)))
                return err;
            if (holesz >= (size_t)incr)
            {
                r->end = r->start + newsz - 1;
                return 0;
            }
            return -ENOMEM;
        }

        if (newsz < 0) /*Expansion would cause underflow*/
            return -EINVAL;
        else if (newsz == 0) /*Region truncated(collapsed), no need of keeping the mapping*/
            return mmap_remove(mmap, r);

        /*Reduce the size of the region*/
        r->end -= oldsz - newsz;
        return 0;
    }
    else if (__vmr_growsdown(r))
    {
        if (newsz > oldsz)
        {
            hole_addr = r->start - incr;
            if ((err = mmap_holesize(mmap, hole_addr, &holesz)))
                return err;
            if (holesz >= (size_t)incr)
            {
                r->start = hole_addr;
                return 0;
            }
            return -ENOMEM;
        }

        if (newsz < 0) /*Expansion would cause underflow*/
            return -EINVAL;
        else if (newsz == 0) /*Region truncated(collapsed), no need of keeping the mapping*/
            return mmap_remove(mmap, r);

        /*Reduce the size of the region*/
        r->start += oldsz - newsz;
        return 0;
    }

    return -ENOMEM;
}

int mmap_protect(mmap_t *mmap, uintptr_t addr, size_t len, int prot)
{
    int err = 0;
    int forge_prot = 0;
    uintptr_t end = addr + len - 1;
    vmr_t *r = NULL, *split0 = NULL, *split1 = NULL, tmp = {0};
    
    if (mmap == NULL || len == 0)
        return -EINVAL;

    mmap_assert_locked(mmap);

    if (!__isaligned(addr) || !__isaligned(len))
        return -EINVAL;

    if (!__valid_addr(addr))
        return -ENOMEM;

    if ((r = mmap_find_vmr_overlap(mmap, addr, end)) == NULL)
        return -ENOMEM;
    
    if (__isstack(r))
        return -EACCES;

    forge_prot |= __vmr_read(r) ? PROT_READ : 0;
    forge_prot |= __vmr_write(r) ? PROT_WRITE : 0;
    forge_prot |= __vmr_exec(r) ? PROT_EXEC : 0;

    if (forge_prot == prot)
        return 0;

    if (__prot_write(prot) && __prot_exec(prot))
        return -EACCES;

    if (__prot_write(prot) && __prot_exec(forge_prot))
        return -EACCES;

    if (__prot_exec(prot) && __prot_write(forge_prot))
        return -EACCES;
        
    if (len < __vmr_size(r))
    {
        if ((err = vmr_alloc(&split0)))
            return err;
        
        tmp = *split0 = *r;
        split0->refs = 0;
        split0->prev = split0->next = NULL;

        if (r->start == addr)
        {
            r->end = end;
            split0->start = __vmr_upper_bound(r);
            if ((err = mmap_mapin(mmap, split0)))
            {
                r->end = split0->end;
                vmr_free(split0);
                return err;
            }
        }
        else if (r->end == end)
        {
            r->start = addr;
            split0->end = __vmr_lower_bound(r);

            if ((err = mmap_mapin(mmap, split0)))
            {
                r->start = split0->start;
                vmr_free(split0);
                return err;
            }
        }
        else
        {
            if ((err = vmr_alloc(&split1)))
            {
                vmr_free(split0);
                return err;
            }

            *split1 = *r;
            split0->refs = 0;
            split1->prev = split1->next = NULL;

            r->start = addr;
            split0->end = __vmr_lower_bound(r);
            r->end = end;
            split1->start = __vmr_upper_bound(r);

            if ((err = mmap_mapin(mmap, split0)))
            {
                *r = tmp;
                vmr_free(split0);
                vmr_free(split1);
                return err;
            }

            if ((err = mmap_mapin(mmap, split1)))
            {
                *r = tmp;
                mmap_remove(mmap, split0);
                vmr_free(split0);
                vmr_free(split1);
                return err;
            }
        }
    }

    __vmr_mask_rwx(r);

    r->flags |= __prot_read(prot) ? VM_READ : 0;
    r->flags |= __prot_exec(prot) ? VM_EXEC : 0;
    r->flags |= __prot_write(prot) ? VM_WRITE : 0;

    return 0;
}

int mmap_clean(mmap_t *mmap)
{
    int err = 0;
    uintptr_t pgdir = 0;
    spinlock_t *lock = NULL;

    if (mmap == NULL)
        return -EINVAL;
    
    mmap_assert_locked(mmap);

    if ((err = mmap_unmap(mmap, 0, (mmap->limit) + 1)))
        return err;

    paging_proc_unmap(mmap->pgdir);
    lock = mmap->lock;
    pgdir = mmap->pgdir;

    memset(mmap, 0, sizeof *mmap);

    mmap->lock = lock;
    mmap->pgdir = pgdir;
    mmap->flags = MMAP_USER;
    mmap->guard_len = PAGESZ;
    mmap->limit = __mmap_limit;

    return 0;
}

void mmap_free(mmap_t *mmap)
{
    int locked = 0;
    if (mmap == NULL)
        return;

    locked = spin_trylock(mmap->lock);
    mmap_clean(mmap);
    if (mmap->refs <= 0)
    {
        if (mmap->pgdir)
            pmman.free(mmap->pgdir);
        
        if (mmap->lock)
            spinlock_free(mmap->lock);
        
        kfree(mmap);
    }

    if (locked)
        mmap_unlock(mmap);
}

int mmap_copy(mmap_t *dst, mmap_t *src)
{
    int err = 0;
    vmr_t *vmr = NULL;

    if (dst == NULL || src == NULL)
        return-EINVAL;

    mmap_assert_locked(src);
    mmap_assert_locked(dst);

    dst->flags = src->flags;
    dst->guard_len = src->guard_len;
    dst->limit = src->limit;
    
    dst->refs = 0;
    dst->priv = NULL;
    dst->used_space = 0;
    dst->heap = dst->env = NULL;
    dst->vmr_head = dst->vmr_tail = NULL;

    forlinked(tmp, src->vmr_head, tmp->next)
    {
        if ((err = vmr_clone(tmp, &vmr)))
            return err;
        if ((err = mmap_mapin(dst, vmr)))
        {
            vmr_free(vmr);
            return err;
        }
        vmr = NULL;
    }

    if (src->heap)
        dst->heap = mmap_find(dst, src->heap->start);
    if (src->env)
        dst->env = mmap_find(dst, src->env->start);

    if ((err = paging_lazycopy(dst->pgdir, src->pgdir)))
        return err;

    return 0;
}

int mmap_clone(mmap_t *mmap, mmap_t **pclone)
{
    int err = 0;
    mmap_t  *clone = NULL;

    if (mmap == NULL || pclone == NULL)
        return -EINVAL;

    mmap_assert_locked(mmap);

    *pclone = NULL;

    if ((err = mmap_alloc (&clone)))
        return err;

    mmap_lock(clone);

    if ((err = mmap_copy(clone, mmap)))
    {
        mmap_free(clone);
        return err;
    }

    *pclone = clone;
    mmap_unlock(clone);

    return 0;
}

void vmr_free(vmr_t *r)
{
    if (r == NULL)
        return;
    if (r->refs <= 0)
    {
        *r = (vmr_t){0};
        kfree(r);
    }
}

int vmr_alloc(vmr_t **ref)
{
    vmr_t *r = NULL;
    if ((r = kmalloc(sizeof *r)) == NULL)
        return -ENOMEM;
    memset(r, 0, sizeof *r);
    *ref = r;
    return 0;
}

int vmr_can_split(vmr_t *r, uintptr_t addr)
{
    if (r == NULL)
        return 0;
    return r->start < addr && addr < r->end;
}

int vmr_split(vmr_t *r, uintptr_t addr, vmr_t **pvmr)
{
    int err = 0;
    vmr_t *new = NULL;
    if (r == NULL || !vmr_can_split(r, addr))
        return -EINVAL;
    
    *pvmr = NULL;

    if ((err = vmr_alloc(&new)))
        return err;

    new->end = r->end;
    new->start = addr;
    new->file = r->file;
    new->flags = r->flags;
    new->vflags = r->vflags;

    if (new->file)
        new->file_pos += addr - r->start;

    r->end = addr - 1;

    if ((err = mmap_mapin(r->mmap, new)))
    {
        vmr_free(new);
        return err;
    }

    if (pvmr)
        *pvmr = new;
    return 0;
}

int vmr_copy(vmr_t *rdst, vmr_t *rsrc)
{
    if (rdst == NULL || rsrc == NULL)
        return -EINVAL;

    if (rdst->mmap && rdst->mmap == rsrc->mmap)
        return -EINVAL;
    
    *rdst = *rsrc;
    rdst->refs = 0;
    rdst->mmap = NULL;
    rdst->priv = NULL;
    rdst->next = rdst->prev = NULL;
    return 0;
}

int vmr_clone(vmr_t *src, vmr_t **pclone)
{
    int err = 0;
    vmr_t *clone = NULL;

    if (src == NULL || pclone == NULL)
        return -EINVAL;
    
    *pclone = NULL;

    if ((err = vmr_alloc(&clone)))
        return err;
    
    if ((err = vmr_copy(clone, src)))
    {
        vmr_free(clone);
        return err;
    }

    *pclone = clone;
    return 0;
}

