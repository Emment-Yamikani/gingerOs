#ifndef LAPIC_H
#define LAPIC_H
#include <lib/stdint.h>

extern volatile uint32_t *lapic;
extern volatile uint32_t *ioapic;
extern volatile int ioapic_id;
extern volatile int disable_pic;

int cpuid(void);
void lapic_eoi(void);
void lapic_init(void);
void lapic_timerintr(void);
void lapic_startup(int id, uint16_t addr);
void lapic_send_ipi_to_all_not_self(int ipi);
#endif //LAPIC_H