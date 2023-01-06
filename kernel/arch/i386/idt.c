#include "isrs.h"
#include <arch/mmu.h>
#include <arch/i386/traps.h>
#include <arch/system.h>
#include <printk.h>

systable_t idtptr;
static gate_desc_t idt[256];

void idt_init(void)
{
    idt_flush(&idtptr);
}

static void idt_setgate(int num, int istrap, uint16_t sel, void (*offset)(), int dpl)
{
    idt[num] = SETGATE(istrap, sel, offset, dpl);
}

void tvinit(void)
{
    idt_setgate(0, 0, SEG_KCODE << 3, isr0, 0);
    idt_setgate(1, 0, SEG_KCODE << 3, isr1, 0);
    idt_setgate(2, 0, SEG_KCODE << 3, isr2, 0);
    idt_setgate(3, 0, SEG_KCODE << 3, isr3, 0);
    idt_setgate(4, 0, SEG_KCODE << 3, isr4, 0);
    idt_setgate(5, 0, SEG_KCODE << 3, isr5, 0);
    idt_setgate(6, 0, SEG_KCODE << 3, isr6, 0);
    idt_setgate(7, 0, SEG_KCODE << 3, isr7, 0);
    idt_setgate(8, 0, SEG_KCODE << 3, isr8, 0);
    idt_setgate(9, 0, SEG_KCODE << 3, isr9, 0);
    idt_setgate(10, 0, SEG_KCODE << 3, isr10, 0);
    idt_setgate(11, 0, SEG_KCODE << 3, isr11, 0);
    idt_setgate(12, 0, SEG_KCODE << 3, isr12, 0);
    idt_setgate(13, 0, SEG_KCODE << 3, isr13, 0);

    idt_setgate(14, 0, SEG_KCODE << 3, isr14, 0);
    idt_setgate(15, 0, SEG_KCODE << 3, isr15, 0);
    idt_setgate(16, 0, SEG_KCODE << 3, isr16, 0);
    idt_setgate(17, 0, SEG_KCODE << 3, isr17, 0);
    idt_setgate(18, 0, SEG_KCODE << 3, isr18, 0);
    idt_setgate(19, 0, SEG_KCODE << 3, isr19, 0);
    idt_setgate(20, 0, SEG_KCODE << 3, isr20, 0);
    idt_setgate(21, 0, SEG_KCODE << 3, isr21, 0);
    idt_setgate(22, 0, SEG_KCODE << 3, isr22, 0);
    idt_setgate(23, 0, SEG_KCODE << 3, isr23, 0);
    idt_setgate(24, 0, SEG_KCODE << 3, isr24, 0);
    idt_setgate(25, 0, SEG_KCODE << 3, isr25, 0);
    idt_setgate(26, 0, SEG_KCODE << 3, isr26, 0);
    idt_setgate(27, 0, SEG_KCODE << 3, isr27, 0);
    idt_setgate(28, 0, SEG_KCODE << 3, isr28, 0);
    idt_setgate(29, 0, SEG_KCODE << 3, isr29, 0);
    idt_setgate(30, 0, SEG_KCODE << 3, isr30, 0);
    idt_setgate(31, 0, SEG_KCODE << 3, isr31, 0);

    idt_setgate(32, 0, SEG_KCODE << 3, irq0, 00);
    idt_setgate(33, 0, SEG_KCODE << 3, irq1, 00);
    idt_setgate(34, 0, SEG_KCODE << 3, irq2, 00);
    idt_setgate(35, 0, SEG_KCODE << 3, irq3, 00);
    idt_setgate(36, 0, SEG_KCODE << 3, irq4, 00);
    idt_setgate(37, 0, SEG_KCODE << 3, irq5, 00);
    idt_setgate(38, 0, SEG_KCODE << 3, irq6, 00);
    idt_setgate(39, 0, SEG_KCODE << 3, irq7, 00);
    idt_setgate(40, 0, SEG_KCODE << 3, irq8, 00);
    idt_setgate(41, 0, SEG_KCODE << 3, irq9, 00);
    idt_setgate(42, 0, SEG_KCODE << 3, irq10, 0);
    idt_setgate(43, 0, SEG_KCODE << 3, irq11, 0);
    idt_setgate(44, 0, SEG_KCODE << 3, irq12, 0);
    idt_setgate(45, 0, SEG_KCODE << 3, irq13, 0);
    idt_setgate(46, 0, SEG_KCODE << 3, irq14, 0);
    idt_setgate(47, 0, SEG_KCODE << 3, irq15, 0);

    idt_setgate(T_SYSCALL, 1, SEG_KCODE << 3, isr128, DPL_USER);
    idt_setgate(T_TLB_SHOOTDOWN, 0, SEG_KCODE << 3, irq40, 0);

    idtptr.lim = sizeof(idt) - 1;
    idtptr.base = (uint32_t)&idt;
}