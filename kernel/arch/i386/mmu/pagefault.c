#include <printk.h>
#include <arch/i386/32bit.h>
#include <bits/errno.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <arch/i386/paging.h>
#include <lib/string.h>
#include <arch/i386/cpu.h>
#include <sys/thread.h>
#include <sys/system.h>
#include <arch/system.h>
#include <arch/context.h>
#include <mm/mmap.h>
#include <arch/sys/signal.h>
#include <sys/proc.h>
#include <arch/i386/traps.h>
#include <sys/sysproc.h>
#include <lime/jiffies.h>
#include <mm/mm_zone.h>
#include <mm/mapping.h>
#include <fs/inode.h>
#include <mm/usermap.h>

int default_pgf_handler(vmr_t *region, vm_fault_t *vm);

void paging_tbl_shootdown()
{
    write_cr3(read_cr3());
}

void do_page_fault(trapframe_t *tf)
{
    int err = 0;
    int in_user = 0;
    mmap_t *mmap = NULL;
    vmr_t *region = NULL;
    vm_fault_t fault = {0};

    pushcli();

    fault.page = NULL;
    fault.flags = tf->eno;
    fault.addr = read_cr2();
    in_user = trapframe_isuser(tf);
    fault.COW = paging_getmapping(fault.addr);

    if (current)
    {
        if ((tf->eip == 0xDEADDEAD) && in_user)
        {
            if ((__thread_ishandling_signal(current)))
                arch_return_signal(tf);
            else if (proc && (current == proc->tmain))
                exit(tf->eax);
            else
                thread_exit(tf->eax);
            goto done;
        }

        current_lock();
        mmap = current->mmap;
        current_unlock();
    }

    //printk("%s: %s(%p): %s: @eip: %p\n", proc ? proc->name : "kernel", __func__, fault.addr, in_user ? "user" : "kernel", tf->eip);

    if (mmap == NULL)
        goto kernel_fault;

    if (!ISKERNEL_ADDR(fault.addr))
    {
        mmap_lock(mmap);
        if ((region = mmap_find(mmap, fault.addr)) == NULL)
        {
            mmap_unlock(mmap);
            goto send_SIGSEGV;
        }

        if (!region->vmops || !region->vmops->fault)
        {
            if ((err = default_pgf_handler(region, &fault)) == -EFAULT)
            {
                mmap_unlock(mmap);
                goto send_SIGBUS;
            } else if (err)
            {
                mmap_unlock(mmap);
                goto send_SIGSEGV;
            }
        }
        else if ((err = region->vmops->fault(region, &fault)))
        {
            mmap_unlock(mmap);
            goto send_SIGSEGV;
        }

        mmap_unlock(mmap);
        goto done;
    }


kernel_fault:
    if (in_user)
        goto send_SIGSEGV;

    panic("%s(%p): %s: %p: eno: %d\n", __func__, fault.addr, in_user ? "user" : "kernel", tf->eip, tf->eno);
    if (proc)
        kill(getpid(), SIGSEGV);

    printk("END\n");
done:
    popcli();
    return;

send_SIGBUS:
    printk("%s(%p): tid: %d, in %s space: eip: %p\n", __func__, fault.addr, thread_self(), in_user ? "user" : "kernel", tf->eip);
    
    if (proc)
        kill(getpid(), SIGBUS);
    else
        panic("%s:%d: Kernel SIGBUS\n", __FILE__, __LINE__);

    return;
send_SIGSEGV:
    
    printk("%s(%p): tid: %d, in %s space: eip: %p\n", __func__, fault.addr, thread_self(), in_user ? "user" : "kernel", tf->eip);
    if (proc)
        kill(getpid(), SIGSEGV);
    else
        panic("%s:%d: Kernel SIGSEGV\n", __FILE__, __LINE__);
}

int default_pgf_handler(vmr_t *region, vm_fault_t *vm)
{
    int err = 0;
    int flags = 0;
    size_t size = 0;
    off_t offset = 0;
    char buf[PAGESZ];
    int frame_refs = 0;
    page_t *page = NULL;
    shared_mapping_t *shared __unused = NULL;
    uintptr_t frame = 0, copy_frame = 0;

    if (region == NULL || vm == NULL)
        return -EINVAL;

    offset = PGROUND(vm->addr) - region->start + region->file_pos;

    if (vm->flags & VM_W)
    {
        if (!__vmr_write(region))
            return -EACCES;

        if (vm->COW && !__vmr_shared(region))
        {
            // copy on write
            frame = PGROUND(vm->COW->raw);
            //frames_lock();

            if ((frame_refs = __page_count(frame)) > 1)
            {
                //frames_unlock();
                if ((copy_frame = pmman.alloc()) == 0)
                    return -ENOMEM;
                //frames_lock();
                flags |= vm->COW->raw & PAGEMASK;
                flags |= region->vflags;
                vm->COW->raw = 0;
                paging_invlpg(vm->addr);
                paging_memcpypp(copy_frame, frame, PAGESZ);
                paging_identity_map(copy_frame, PGROUND(vm->addr), PAGESZ, flags);
                send_tlb_shootdown();
                __page_put(frame);
            }
            else if (frame_refs == 1)
            {
                vm->COW->raw |= VM_W;
                paging_invlpg(PGROUND(vm->addr));
                send_tlb_shootdown();
            }
            else
                panic("%s:%d: PGF(%p)\n", __FILE__, __LINE__, vm->addr);
            //frames_unlock();
            return 0;
        }

        if (vm->flags & VM_P)
        {
            printk("page fault @ probably shared page\n");
            return -EACCES;
        }

        region->vflags |= VM_W | VM_P;

        if (region->file)
        {
            if (region->file->i_size == 0)
                return -EFAULT;

            if (!__vmr_shared(region)) {
                if ((frame = pmman.alloc()) == 0)
                    return -ENOMEM;
                size = __min(PAGESZ, __min(region->filesz, region->file->i_size - offset));
                memset(buf, 0, PAGESZ);
                iread(region->file, offset, buf, size);
                paging_memcpyvp(frame, (uintptr_t)buf, PAGESZ);
                if ((err = paging_identity_map(frame, PGROUND(vm->addr), PAGESZ, region->vflags)))
                {
                    pmman.free(frame);
                    return err;
                }
            }
            else {
                err = inode_getpage(region->file, offset / PAGESZ, &frame, &page);
                //printk("offset: %dframe: %p, err: %d\n", offset, frame, err);
                __page_incr(frame);
                // if ((err = shared_mapping_alloc(region->mmap, region->file, offset / PAGESZ, PGROUND(vm->addr), &shared)))
                // return err;

                if ((err = paging_identity_map(frame, PGROUND(vm->addr), PAGESZ, region->vflags)))
                    return err;
                // panic("%s: %d: shared page\n", __FILE__, __LINE__);
            }
            send_tlb_shootdown();
        }
        else
        {
            if ((err = paging_mappages(PGROUND(vm->addr), PAGESZ, region->vflags)))
                return err;
            if (__vmr_zero(region))
                memset((void *)PGROUND(vm->addr), 0, PAGESZ);
            send_tlb_shootdown();
        }
        return 0;
    }

    // printk("addr: %p: vflags: %d\n", vm->addr, region->vflags);
    if (!__vmr_read(region) && !__vmr_exec(region))
        return -EACCES;

    if (vm->flags & VM_P)
        return -EACCES;

    if (region->file)
    {
        if (region->file->i_size == 0)
            return -EFAULT;

        if (!__vmr_shared(region)) {
            if ((frame = pmman.alloc()) == 0)
                return -ENOMEM;
            size = __min(PAGESZ, __min(region->filesz, region->file->i_size - offset));
            memset(buf, 0, PAGESZ);
            iread(region->file, offset, buf, size);
            paging_memcpyvp(frame, (uintptr_t)buf, PAGESZ);
            if ((err = paging_identity_map(frame, PGROUND(vm->addr), PAGESZ, region->vflags)))
            {
                pmman.free(frame);
                return err;
            }
        }
        else
        {
            err = inode_getpage(region->file, offset / PAGESZ, &frame, &page);
            // printk("offset: %dframe: %p, err: %d\n", offset, frame, err);
            __page_incr(frame);
            // if ((err = shared_mapping_alloc(region->mmap, region->file, offset / PAGESZ, PGROUND(vm->addr), &shared)))
                // return err;
            
            if ((err = paging_identity_map(frame, PGROUND(vm->addr), PAGESZ, region->vflags)))
                return err;
            // panic("%s: %d: shared page\n", __FILE__, __LINE__);
        }
        send_tlb_shootdown();
    }
    else
    {
        if ((frame = pmman.alloc()) == 0)
            return -ENOMEM;
        if (__vmr_zero(region))
        {
            memset(buf, 0, PAGESZ);
            paging_memcpyvp(frame, (uintptr_t)buf, PAGESZ);
        }
        if ((err = paging_identity_map(frame, PGROUND(vm->addr), PAGESZ, region->vflags)))
        {
            pmman.free(frame);
            return err;
        }
        send_tlb_shootdown();
    }

    return 0;
}