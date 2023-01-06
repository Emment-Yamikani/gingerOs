#include <arch/mmu.h>
#include <lib/string.h>
#include <arch/system.h>
#include <arch/i386/cpu.h>
#include <arch/i386/traps.h>
#include <arch/sys/thread.h>
#include <arch/i386/paging.h>

volatile int ncpu = 0;
cpu_t cpus[NCPU] = {0};
atomic_t cpus_online = {0};
cpu_t *bootstrap_cpu = NULL;

void set_tss(uint32_t esp, int ss)
{
    cpu->gdt[SEG_TSS] = SEG(0x0C, 0xE9, (uint32_t)&cpu->tss, sizeof cpu->tss - 1);
    memset(&cpu->tss, 0, sizeof cpu->tss);
    cpu->tss.esp0 = esp;
    cpu->tss.ss0 = ss;
    tss_flush((SEG_TSS << 3) | 3);
}

void setupsegs(void)
{
    disable_caching();
    cpu_t*c = &cpus[cpuid()];
    
    c->gdt[SEG_NULL] =  SEG(0x00, 0x00, 0x00000000, 0x00000000);
    c->gdt[SEG_KCODE] = SEG(0x0C, 0x9A, 0x00000000, 0xffffffff);
    c->gdt[SEG_KDATA] = SEG(0x0C, 0x92, 0x00000000, 0xffffffff);
    c->gdt[SEG_UCODE] = SEG(0x0C, 0xFA, 0x00000000, 0xffffffff);
    c->gdt[SEG_UDATA] = SEG(0x0C, 0xF2, 0x00000000, 0xffffffff);
    c->gdtptr = (systable_t){.base= (uint32_t)c->gdt, .lim=sizeof c->gdt - 1};
    c->gdt[SEG_KCPU] = SEG(0x0C, 0x92, (uint32_t)&c->cpu, (sizeof *c - ((int)&c->cpu - (int)&c->ncli)) - 1);
    
    gdt_flush(&c->gdtptr);
    loadgs((SEG_KCPU << 3));

    cpu = c;
    proc = NULL;
    current = NULL;
    ready_queue = NULL;
}

int cpu_init(void)
{
    cli();          //ensure interrupts are disabled
    setupsegs();    //initialize segement registers
    idt_init();     //load interrupt descriptor
    lapic_init();   //initialize the local interrupt controller

    cpu->ncli = 0;
    cpu->intena = 0;
    atomic_incr(&cpus_online);
    return 0;
}

int ap_init(void)
{
    return cpu_init();
}