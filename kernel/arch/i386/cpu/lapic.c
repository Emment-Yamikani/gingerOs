#include <arch/i386/lapic.h>
#include <arch/i386/traps.h>
#include <lime/preempt.h>
#include <arch/mmu.h>
#include <arch/system.h>
#include <printk.h>
#include <lime/assert.h>
#include <locks/atomic.h>
#include <sys/thread.h>
#include <arch/i386/cpu.h>

volatile uint32_t *lapic = (uint32_t *)0xFEC00000;

#define LAPIC_ID lapic[(0x020 / 4)]      // Read/Write
#define LAPIC_VERSION lapic[(0x030 / 4)] // Read only
#define TPR lapic[(0x080 / 4)]           // Read/Write
#define APR lapic[(0x090 / 4)]           // Read only
#define PPR lapic[(0x0A0 / 4)]           // Read only
#define EOI lapic[(0x0B0 / 4)]           // Write_only
#define RRR lapic[(0x0C0 / 4)]           // Read only
#define LDR lapic[(0x0D0 / 4)]           // Read/Write
#define DFR lapic[(0x0E0 / 4)]           // Read/Write
#define SIR lapic[(0x0F0 / 4)]           // Read/Write
#define ISR lapic[(0x100 / 4)]           //- 170h // Read only
#define TMR lapic[(0x180 / 4)]           //- 1F0h // Read only
#define IRR lapic[(0x200 / 4)]           //- 270h // Read only
#define ESR lapic[(0x280 / 4)]           // Read only
#define LVT_CMCI lapic[(0x2F0 / 4)]      // Read/Write
#define ICR lapic[(0x300 / 4)]           //- 310h // Read/Write
#define ICRHI lapic[(0x310 / 4)]         // Read /Write
#define LVT_TR lapic[(0x320 / 4)]        // Read/Write
#define LVT_TSR lapic[(0x330 / 4)]       // Read/Write
#define LVT_PMCR lapic[(0x340 / 4)]      // Read/Write
#define LVT_LINT0 lapic[(0x350 / 4)]     // Read/Write
#define LVT_LINT1 lapic[(0x360 / 4)]     // Read/Write
#define LVT_Error lapic[(0x370 / 4)]     // Read/Write
#define TICR lapic[(0x380 / 4)]          // Read/Write
#define TCCR lapic[(0x390 / 4)]          // Read only
#define TDCR lapic[(0x3E0 / 4)]          // Read/Write

#define ENABLE 0x00000100   // Unit Enable
#define INIT 0x00000500     // INIT/RESET
#define STARTUP 0x00000600  // Startup IPI
#define DELIVS 0x00001000   // Delivery status
#define ASSERT 0x00004000   // Assert interrupt (vs deassert)
#define LEVEL 0x00008000    // Level triggered
#define BCAST 0x00080000    // Send to all APICs, including self.
#define X1 0x0000000B       // divide counts by 1
#define PERIODIC 0x00020000 // Periodic
#define MASKED 0x00010000   // Interrupt masked

#define LAPIC_SHORTHAND_NO            (0x00000)
#define LAPIC_SHORTHAND_SELF          (0xA0000)
#define LAPIC_SHORTHAND_ALL           (0xB0000)
#define LAPIC_SHORTHAND_EXCEPT_SELF   (0xC0000)

int local_id(void)
{
    if ((read_eflags() & FL_IF))
        panic("interruptable called @ 0x%p\n", return_address(0));
    return (int)(LAPIC_ID >> 24);
}

int cpuid(void)
{
    if ((read_eflags() & FL_IF))
        panic("interruptable called @ 0x%p\n", return_address(0));
    return (int)(LAPIC_ID >> 24);
}

void lapic_eoi(void)
{
    if (lapic)
        EOI = 0;
}

static void microdelay(int us)
{
    while (us--)
    {
        for (int i = 0; i < 1000; i++)
            ;
    }
}

void lapic_init(void)
{
    SIR = ENABLE | (IRQ_OFFSET + IRQ_SPURIOUS);

    LVT_TR = MASKED;
    TDCR = X1;
    TICR = 1000000;
    LVT_TR = PERIODIC | (IRQ_OFFSET + IRQ_TIMER);

    LVT_LINT0 = MASKED;
    LVT_LINT1 = MASKED;

    LVT_Error = IRQ_OFFSET + IRQ_ERROR;

    ESR = 0;
    ESR = 0;
    lapic_eoi();

    ICRHI = 0;
    ICR = LEVEL | BCAST | INIT;
    while (ICR & DELIVS)
        ;

    TPR = 0;
}

void lapic_startup(int id, uint16_t addr)
{
    ICRHI = (id << 24);
    ICR = INIT | ASSERT | LEVEL;
    while (ICR & DELIVS)
        ;

    ICRHI = (id << 24);
    ICR = ICR = INIT | LEVEL;
    while (ICR & DELIVS)
        ;

    // intel says wait for 10us after INIT
    microdelay(10);

    for (int i = 0; i < 2; i++)
    {
        ICRHI = (id << 24);
        ICR = STARTUP | (addr >> 12);
        while (ICR & DELIVS)
            ;
        // intel says wait for 200us aftar SIPI
        microdelay(200);
    }
}

void lapic_send_ipi_to_all_not_self(int ipi)
{
    ICR = (ASSERT | LEVEL) | LAPIC_SHORTHAND_EXCEPT_SELF | (ipi & 0xff);
    while (ICR & DELIVS)
        ;
}

void lapic_timerintr(void)
{
    if (((atomic_incr(&cpu->timer_ticks) % 1) == 0) && current)
        atomic_decr(&current->t_sched_attr.t_timeslice);
}