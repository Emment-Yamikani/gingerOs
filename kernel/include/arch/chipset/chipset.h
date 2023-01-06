#ifndef CHIPSET_H
#define CHIPSET_H

#define IRQ_OFFSET 32
#define IRQ_TIMER   0
#define IRQ_KBD     1
#define IRQ_MOUSE   12
#define IRQ_ERROR   19
#define IRQ_TLB_SHOOT 40

void pic_enable(int irq);
void pic_init(void);
void ioapic_init(void);

void ioapic_enable(int irq, int cpunum);

#endif //CHIPSET_H