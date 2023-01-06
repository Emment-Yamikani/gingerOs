#pragma once

#include "lapic.h"

#define NIOAPIC 8

typedef struct __ioapic__
{
    uint8_t id;
    uint32_t ioapic_addr;
    uint32_t flags;
}__ioapic__t;

extern __ioapic__t apics[NIOAPIC];
extern volatile int ismp;

void bootothers(void);
int mp_process(void);