#include <printk.h>
#include <arch/i386/32bit.h>
#include <bits/errno.h>
#include <arch/system.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <arch/i386/paging.h>
#include <lib/string.h>
#include <arch/i386/cpu.h>
#include <sys/thread.h>
#include <arch/system.h>
#include <arch/context.h>
#include <mm/shm.h>
#include <arch/sys/signal.h>
#include <sys/proc.h>
#include <arch/i386/traps.h>

void paging_pagefault(trapframe_t *tf)
{
    int refs = 0;
    vmr_t *vmr = NULL;
    uintptr_t addr = 0, frame = 0;
    page_t *pte = NULL;
    addr = PGROUND(read_cr2());
    paging_invlpg(addr);

    if (current)
    {
        //printk("%d:%d pagefault: %p\n", current->owner->pid, current->t_tid, read_cr2());
        shm_lock(current->mmap);
        shm_pgdir_lock(current->mmap);
        if ((pte = paging_getmapping(addr)))
        {
            frame = PGROUND(pte->raw);
            if (!(vmr = shm_lookup(current->mmap, addr)))
            {
                shm_pgdir_unlock(current->mmap);
                shm_unlock(current->mmap);
                printk("0-going to segfault\n");
                goto segfault;
            }
            
            //printk("pagefault: vmr: %p, oflags: \e[0;013m0%b\e[0m, vflags: \e[0;012m0%b\e[0m\n", vmr->vaddr, vmr->oflags, vmr->vflags);
            
            vmr_lock(vmr);
            if (!(vmr->oflags & VM_WRITE))
            {
                printk("1-going to read_perm\n");
                goto read_perm;
            }

        read_perm:
            if (!(vmr->oflags & VM_READ))
            {
                vmr_unlock(vmr);
                shm_pgdir_unlock(current->mmap);
                shm_unlock(current->mmap);
                printk("2-going to segfault\n");
                goto segfault;
            }

            frames_lock();
            refs = frames_get_refs(frame / PAGESZ);
            frames_unlock();

            assert(refs, "pageframe null reference count");

            if (refs == 1)
            {
                pte->structure.w = 1;
                send_tlb_shootdown();
                //klog(KLOG_WARN, "%s:%d: [TODO] implement demand paging\n", __FILE__, __LINE__);
            }
            else
            {
                int flags = pte->raw & PAGEMASK;
                pte->raw = 0;
                flags |= vmr->vflags;
                paging_mappages(addr, PAGESZ, flags);
                uintptr_t vaddr = paging_mount(frame);
                memcpy((void *)addr, (void *)vaddr, PAGESZ);
                paging_unmount(vaddr);

                frames_lock();
                frames_decr(frame / PAGESZ);
                frames_unlock();
            }

            paging_invlpg(addr);

            vmr_unlock(vmr);
        }
        else
        {
            //printk("PGF: %p\n", addr);
            if (!(vmr = shm_lookup(current->mmap, addr)))
            {
                shm_pgdir_unlock(current->mmap);
                shm_unlock(current->mmap);
                goto segfault;
            }

            if ((paging_mappages(addr, PAGESZ, vmr->vflags)))
                goto segfault;
        }

        shm_pgdir_unlock(current->mmap);
        shm_unlock(current->mmap);

        return;
    segfault:
        panic("cpu%d: pagefault(SEGFAULT): %p err_code: [0%ph], eip: %p thread(%d)\n",
              cpu->cpuid, read_cr2(), tf->eno, tf->eip, thread_self());
    }
    else
        panic("pagefault: addr: %p\n", read_cr2());
}

void paging_tbl_shootdown()
{
    write_cr3(read_cr3());
}