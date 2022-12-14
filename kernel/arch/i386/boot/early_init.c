#include <printk.h>
#include <arch/i386/paging.h>
#include <arch/boot/boot.h>
#include <arch/i386/cpu.h>
#include <lime/preempt.h>
#include <mm/pmm.h>
#include <fs/fs.h>
#include <locks/spinlock.h>
#include <ds/bitmap.h>
#include <arch/system.h>
#include <arch/chipset/mp.h>
#include <arch/chipset/chipset.h>
#include <mm/kalloc.h>
#include <dev/dev.h>
#include <mm/vmm.h>
#include <arch/chipset/mp.h>
#include <sys/sched.h>
#include <arch/sys/thread.h>
#include <arch/sys/kthread.h>
#include <sys/thread.h>
#include <sys/kthread.h>
#include <arch/boot/early.h>

void *kthread_main(void *);

int early_init(void)
{
    int err =0;
    if ((err = pmman.init()))
        return err;

    if ((err = mp_process()))
        return err;

    if ((err = vmman.init()))
        return err;

    pic_init();
    ioapic_init();
    lapic_init();

    if ((err = dev_init()))
        return err;

    if ((err = vfs_init()))
        return err;

    //boot other cores if present
    bootothers();

    paging_unmap_table(0);
    paging_unmap_table(1);
    paging_unmap_table(2);
    paging_unmap_table(3);

    //signal others cores
    atomic_xchg(&cpu->online, 1);

    kthread_create(kthread_main, NULL, NULL, NULL);
    // start executing scheduler
    schedule();
    return 0;
}