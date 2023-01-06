#ifndef ARCH_SYSTEM_H
#define ARCH_SYSTEM_H
#include <lib/stdint.h>

static inline void hlt(void){
    asm __volatile__("hlt");
}

static inline void cli(void)
{
    asm __volatile__("cli");
}

static inline void sti(void)
{
    asm __volatile__("sti");
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t data;
    __asm__ __volatile__("inb %1, %0"
                         : "=a"(data)
                         : "dN"(port));
    return data;
}

static inline void outb(uint16_t port, uint8_t data)
{
    __asm__ __volatile__("outb %1, %0"
                         :
                         : "dN"(port), "a"(data));
}

static inline void
loadgs(uint16_t v)
{
  asm volatile("movw %0, %%gs" : : "r" (v));
}

static inline uint32_t xchg(volatile uint32_t *addr, uint32_t newval)
{
    uint32_t result;
    // The + in "+m" denotes a read−modify−write operand.
    asm volatile("lock; xchgl %0, %1" :  "+m"(*addr), "=a"(result) :  "1"(newval) :  "cc");
    return result;
}

struct systable;

void disable_caching(void);

void
gdt_flush(struct systable *);
void
idt_flush(struct systable *);
void
tss_flush(uint32_t);

uint32_t read_tsc(void);

void _kstart(void);
void _ustart(void);

extern void
trapret(void);

extern void
swtch();

extern uint32_t _xchg_(volatile uint32_t *ptr, uint32_t newval);
extern void write_cr3(uint32_t);
extern uint32_t read_cr3(void);
extern uint32_t read_cr2(void);
extern uint32_t read_esp(void);
extern uint32_t read_ebp(void);
extern uint32_t read_eax(void);
extern uint32_t read_eflags(void);

#endif //ARCH_SYSTEM_H