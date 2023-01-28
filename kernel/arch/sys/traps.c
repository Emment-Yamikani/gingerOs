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
#include <arch/sys/signal.h>
#include <arch/i386/paging.h>
#include <mm/kalloc.h>
#include <locks/spinlock.h>
#include <sys/proc.h>
#include <sys/sysproc.h>

void rtc_intr(void);
void kbd_intr(void);

spinlock_t *biglock = SPINLOCK_NEW("biglock");

void ps2mouse_handler(void);
void paging_pagefault(trapframe_t *tf);
void paging_tbl_shootdown();

void trap(trapframe_t *tf)
{
    if (current){
        if (thread_iskilled(current)){
            thread_exit(-ERFKILL);
        }
    }

    switch (tf->ino){
    case T_SYSCALL: // syscall
        current_assert();
        current->t_tarch->tf = tf;
        // printk("syscall ");
        syscall_stub(tf);
        // printk(" sysret\n");

        if (thread_iskilled(current)){
            thread_exit(-ERFKILL);
        }
        break;
    case T_LOCAL_TIMER: // Local APIC timer
        lapic_timerintr();
        lapic_eoi();
        break;
    case T_KBD0: // PS/2 keyboard
        kbd_intr();
        lapic_eoi();
        break;
    case T_RTC_TIMER: // Real Time Clock
        rtc_intr();
        lapic_eoi();
        break;
    case T_PS2MOUSE: // PS/2 mouse
        ps2mouse_handler();
        lapic_eoi();
        break;
    case T_PGFAULT: // Pagefault
        pushcli();
        if ((tf->eip == 0xDEADDEAD) && trapframe_isuser(tf)) {
            if ((thread_ishandling_signal(current)))
                arch_return_signal(tf);
            else
                exit(tf->eax);
        }
        else
            paging_pagefault(tf);
        lapic_eoi();
        popcli();
        break;
    case T_TLB_SHOOTDOWN: // TLB shootdown
        paging_tbl_shootdown();
        lapic_eoi();
        break;
    default:
        panic("exception(%d), addr: %p, err_code=0%ph\n", tf->ino, read_cr2(), tf->eno);
    }

    if (current == NULL)
        return;

    if (thread_iskilled(current)){
        thread_exit(-ERFKILL);
    }

    if (atomic_read(&current->t_sched_attr.t_timeslice))
        sched_yield();

    if (thread_iskilled(current)){
        thread_exit(-ERFKILL);
    }

    if (proc == NULL)
        return;

    pushcli();
    if (trapframe_isuser(tf))
        handle_signals(tf);
    popcli();
}

void send_tlb_shootdown(void)
{
    lapic_send_ipi_to_all_not_self(T_TLB_SHOOTDOWN);
}

void dump_trapframe(trapframe_t *tf)
{
    printk("\n\t\t\tTRAPFRAME\n"
           "edi: %08p, esi: %08p, ebp: %08p\n"
           "esp: %08p, ebx: %08p, edx: %08p\n"
           "ecx: %08p, eax: %08p, _gs: %08p\n"
           "_fs: %08p, _es: %08p, _ds: %08p\n"
           "ino: %08p, eno: %08p, eip: %08p\n"
           "_cs: %08p, EFL: %08p, esp: %08p, _ss: %0p\n",
           tf->edi, tf->esi, tf->ebp, tf->temp_esp,
           tf->ebx, tf->edx, tf->ecx, tf->eax, tf->gs,
           tf->fs, tf->es, tf->ds, tf->ino, tf->eno,
           tf->eip, tf->cs, tf->eflags, tf->esp, tf->ss);
}