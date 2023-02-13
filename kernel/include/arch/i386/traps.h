#ifndef TRAPS_H
#define TRAPS_H

#define IRQ_OFFSET 32
#define IRQ_TIMER   0
#define IRQ_KBD     1
#define IRQ_HPET    2
#define IRQ_MOUSE   12
#define IRQ_ERROR   19
#define IRQ_RTC     8
#define IRQ_SPURIOUS 31
#define IRQ_TLB_SHOOT 40

#define T_KBD0      (IRQ_KBD + IRQ_OFFSET)
#define T_HPET      (IRQ_OFFSET + IRQ_HPET)
#define T_LOCAL_TIMER (IRQ_OFFSET + IRQ_TIMER)
#define T_RTC_TIMER   (IRQ_OFFSET + IRQ_RTC)
#define T_PS2MOUSE (IRQ_OFFSET + IRQ_MOUSE)
#define T_TLB_SHOOTDOWN  (IRQ_TLB_SHOOT + IRQ_OFFSET)


#define T_PGFAULT   14
#define T_SYSCALL   0x80

void tvinit(void);
void idt_init(void);

void send_tlb_shootdown(void);


#endif //TRAPS_H