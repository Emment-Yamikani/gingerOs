#include <lib/stdint.h>
#include <arch/chipset/chipset.h>
#include <printk.h>
#include <arch/chipset/mp.h>

#define IOAPICREG 0x0

#define IOAPIC_DATA 0x4

/**
 * IOAPIC REGISTERS
*/

#define IOAPICID 0x0
#define IOAPICVER 0x1
#define INT_MASK 0x10000
#define REG_TABLE(n) (0x10 + 2 * n)

volatile uint32_t *ioapic =0;
volatile int ioapic_id =0;

static uint32_t ioapic_read(int reg)
{
    ioapic[IOAPICREG] = reg & 0xff;
    return ioapic[IOAPIC_DATA];
}

void ioapic_write(int reg, uint32_t data)
{
    ioapic[IOAPICREG] = reg & 0xff;
    ioapic[IOAPIC_DATA] = data;
}

void ioapic_init()
{
    int id, maxints;

    if (!ismp)
    {
        printk("machine is not mp\n");
        return;
    }

    id = (ioapic_read(IOAPICID) >> 24) & 0xff;
    id = id;
    /*if (id != ioapic_id)
        panic("id(%d) doesn't match ioapic(%d)\n", id, ioapic_id);
    */
    //printk("id: %d\n", id);
    // ver = ioapic_read(IOAPICVER) & 0xff;
    maxints = (ioapic_read(IOAPICVER) >> 16) & 0xff;
    //printk("maxints: %d\n", maxints);
    for (int i = 0; i < maxints; i++)
    {
        ioapic_write(REG_TABLE(i), INT_MASK | (IRQ_OFFSET + i));
        ioapic_write(REG_TABLE(i) + 1, 0);
    }
}

void ioapic_enable(int irq, int cpunum)
{
    if (!ismp)
        return;
    // Mark interrupt edge-triggered, active high,
    // enabled, and routed to the given cpunum,
    // which happens to be that cpu's APIC ID.
    ioapic_write(REG_TABLE(irq), IRQ_OFFSET + irq);
    ioapic_write(REG_TABLE(irq) + 1, cpunum << 24);
}