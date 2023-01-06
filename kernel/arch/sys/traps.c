#include <arch/context.h>
#include <arch/i386/traps.h>
#include <arch/i386/lapic.h>
#include <arch/mmu.h>
#include <arch/system.h>
#include <lime/assert.h>
#include <printk.h>
#include <sys/system.h>
#include <arch/i386/cpu.h>
#include <sys/thread.h>
#include <bits/errno.h>
#include <arch/chipset/chipset.h>
#include <sys/_syscall.h>
#include <locks/mutex.h>

void rtc_intr(void);
void kbd_intr(void);

spinlock_t *biglock = SPINLOCK_NEW("biglock");

void paging_pagefault(trapframe_t *tf);
void paging_tbl_shootdown();

void trap(trapframe_t *tf)
{
    if (current)
    {
        if (atomic_read(&current->t_killed))
            thread_exit(-ERFKILL);
    }

    if (tf->ino == T_SYSCALL)
    {
        current_assert();
        current->t_tarch->tf = tf;
        
        //printk("syscall ");
        syscall_stub(tf);
        //printk(" sysret\n");

        if (atomic_read(&current->t_killed))
            thread_exit(-ERFKILL);
        return;
    }

    switch (tf->ino)
    {
    case T_LOCAL_TIMER:
        lapic_timerintr();
        lapic_eoi();
        break;
    case T_KBD0:
        kbd_intr();
        lapic_eoi();
        break;
    case T_RTC_TIMER:
        rtc_intr();
        lapic_eoi();
        break;
    case T_PGFAULT:
        paging_pagefault(tf);
        break;
    case T_TLB_SHOOTDOWN:
        paging_tbl_shootdown();
        lapic_eoi();
        break;
    default:
        panic("exception(%d), addr: %p, err_code=0%ph\n", tf->ino, read_cr2(), tf->eno);
    }

    if (current == NULL)
        return;

    if (atomic_read(&current->t_killed))
        thread_exit(-ERFKILL);

    if (atomic_read(&current->t_sched_attr.t_timeslice) <= 0)
        sched_yield();
}

void send_tlb_shootdown(void)
{
    lapic_send_ipi_to_all_not_self(T_TLB_SHOOTDOWN);
}