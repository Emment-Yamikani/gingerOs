#include <arch/i386/cpu.h>
#include <arch/system.h>
#include <arch/i386/paging.h>
#include <fs/fs.h>
#include <mm/kalloc.h>
#include <arch/chipset/mp.h>
#include <printk.h>
#include <lime/preempt.h>
#include <arch/i386/traps.h>
#include <sys/sched.h>

static void ap_wait(void)
{
    barrier();
    while (atomic_read(&bootstrap_cpu->online) == 0)
        CPU_RELAX();
}

static void bsp_signal(void)
{
    barrier();
    atomic_write(&cpu->online, 1);
}

static void bsp_wait(cpu_t *ap)
{
    barrier();
    while (atomic_read(&ap->online) == 0)
        CPU_RELAX();
}

static void ap_startup(void)
{
    ap_init();
    bsp_signal();
    ap_wait();
    schedule();
}

void bootothers(void)
{
    inode_t *apmod = NULL;
    bootstrap_cpu = cpu;

    if (!ismp)
        return;

    if (ncpu < 2)
    {
        printk("\e[00;5mOnly '1' cpu exists on this machine\e[0m\n");
        return;
    }

    if (vfs_lookup("/ap_module", NULL, O_RDONLY, &apmod, NULL)){
        printk("AP startup module not found\n");
        return;
    }

    if (iread(apmod, 0, TRAMPOLINE, apmod->i_size) != apmod->i_size)
    {
        printk("failed to read AP startup module\n");
        return;
    }

    printk("Starting %d AP(s)...\n", ncpu -1);
    for (cpu_t *ap = cpus; (int)(ap - cpus) < ncpu; ++ap)
    {
        if (ap == cpu)
            continue;
        uint32_t cpu_stack = paging_alloc(MPSTACK) + MPSTACK;
        *((uint32_t *)(TRAMPOLINE + 4092)) = cpu_stack;
        *((uint32_t *)(TRAMPOLINE + 4088)) = (uint32_t)PGROUND(read_cr3());
        *((uint32_t *)(TRAMPOLINE + 4084)) = (uint32_t)ap_startup;
        lapic_startup(ap->cpuid, (uint16_t)(uint32_t)TRAMPOLINE);
        bsp_wait(ap);
        printk("\e[0;012mAP%d started...\e[0m\n", ap->cpuid);
    }
}