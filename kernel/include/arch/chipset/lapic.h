#ifndef LAPIC_H
#define LAPIC_H

#include <lib/stdint.h>
#include <sys/system.h>

#define APIC_PROCESSOR  0
#define IO_APIC         1
#define NMI_SOURCE      3

typedef struct _lapic_
{
    uint8_t type;
    uint8_t length;
    uint8_t acpiid;
    uint8_t apicid;
    uint32_t flags;
}__packed lapic_t;

typedef struct _lapic_flags_
{
    uint32_t enabled: 1;
    uint32_t resvd: 31;
} __packed lapic_flags_t;


typedef struct _ioapic_
{
    uint8_t type;
    uint8_t length;
    uint8_t ioapicid;
    uint8_t resvd;
    uint32_t ioapic_addr;
    /**
     * The global system interrupt number where this ioapic's interrupt
     * input starts. The number of interrupt inputs is determined by the ioapic's
     * Max Redir entry register
     */
    uint32_t global_sys_int_base;
}__packed ioapic_t;

typedef struct _nmi_source_
{
    uint8_t type;
    uint8_t length;
    uint16_t flags;
    uint32_t global_sys_int;
} __packed nmi_source_t;

extern volatile uint32_t *lapic;
extern volatile int disable_pic;
extern volatile int lapic_mapped;
extern volatile int ioapic_id;
extern volatile uint32_t *ioapic;

void lapic_init(void);
void lapic_eoi(void);
void lapic_startup(int id, uint32_t addr);
void lapic_ipi(int id, int vect);
void lapic_send_ipi_to_all_not_self(int ipi);

#endif //LAPIC_H