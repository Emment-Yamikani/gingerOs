#include <printk.h>
#include <mm/kalloc.h>
#include <lib/string.h>
#include <lime/string.h>
#include <dev/hpet.h>
#include <acpi/acpi.h>
#include <dev/dev.h>
#include <fs/devfs.h>
#include <sys/thread.h>
#include <sys/proc.h>
#include <arch/i386/cpu.h>
#include <arch/i386/paging.h>
#include <bits/errno.h>
#include <arch/chipset/chipset.h>
#include <arch/i386/traps.h>
#include <lime/jiffies.h>

typedef struct acpi_hpet
{
    acpiSDT_t HDR;
    uint32_t timer_blockID;
    struct
    {
        uint8_t addr_ID;
        uint8_t register_width;
        uint8_t register_offset;
        uint8_t resvd;
        uint64_t timer_block_addr;
    } __packed base_addr;
    uint8_t hpet_number;
    uint16_t min_clk_tick;
    struct
    {
        uint8_t no_guarantee : 1;
        uint8_t _4kib : 1;
        uint8_t _64kib : 1;
        uint8_t resvd : 4;
    } __packed page_protection;
} __packed acpi_hpet_t;

acpi_hpet_t *acpi_hpet = NULL;

static int ntimer = 0;
static uintptr_t hpet = 0;
static uint32_t hpet_clk = 0;

static struct
{
    uint8_t active;   // is active ?;
    uint8_t _32mode;  // operating in 32 bit mode?
    uint8_t type;     // non-periodic or periodic?
    uint8_t trigger_mode; // Edge or Level-trigered?
} active_timers[32] = {0};

void dump_capID(capID_t ID)
{
    printk("GENERAL CapID REGISTER\n"
           "revision_ID: %d\n"
           "timer_count: %d\n"
           "counter_size: %d\n"
           "resvd: %d\n"
           "legacy_cap: %d\n"
           "vendor_id: %d\n"
           "clock_period: %p\n",
           ID.revision_ID,
           ID.timer_count,
           ID.counter_size,
           ID.resvd,
           ID.legacy_cap,
           ID.vendor_id,
           ID.clock_period);
}

void dump_conf(conf_t conf)
{
    printk("GENERAL CONFIG REGISTER\n"
           "General Enable: %d\n"
           "Legacy/Replacement: %d\n",
           conf.enable,
           conf.legacy_route);
}

void dump_timerconf(hpet_timer_t timer)
{
    printk("TIMER CONFIG REGISTER\n"
           "resvd0: %d\n"
           "trigger_mode: %d\n"
           "int_enable: %d\n"
           "timer_type: %d\n"
           "periodic_cap: %d\n"
           "timer_size: %d\n"
           "value_set: %d\n"
           "resvd1: %d\n"
           "_32mode: %d\n"
           "int_route: %d\n"
           "fsb_enable: %d\n"
           "fsb_int_delivery: %d\n"
           "resvd2: %d\n"
           "int_route_cap: %032b\n",
           timer.conf_cap.resvd0,
           timer.conf_cap.trigger_mode,
           timer.conf_cap.int_enable,
           timer.conf_cap.timer_type,
           timer.conf_cap.periodic_cap,
           timer.conf_cap.timer_size,
           timer.conf_cap.value_set,
           timer.conf_cap.resvd1,
           timer.conf_cap._32mode,
           timer.conf_cap.int_route,
           timer.conf_cap.fsb_enable,
           timer.conf_cap.fsb_int_delivery,
           timer.conf_cap.resvd2,
           timer.conf_cap.int_route_cap);
}

#define HPET_EDGE_TRIGGERED 0
#define HPET_LEVEL_TRIGGERED    1

#define HPET_CAPID_REG (hpet + 0x0)
#define HPET_CONF_REG  (hpet + 0x10)
#define HPET_INT_STS_REG (hpet + 0x20)
#define HPET_COUNTER_REG (hpet + 0xF0)

#define HPET_TIMER(n) (hpet + (0x20 * n))

#define HPET_TIMER_CONF_REG(n) (HPET_TIMER(n) + 0x100)
#define HPET_TIMER_COMP_VAL_REG(n) (HPET_TIMER(n) + 0x108)
#define HPET_TIMER_FSB_ROUTE_REG(n) (HPET_TIMER(n) + 0x110)

uint32_t hpet_read_lo(uint32_t *addr){ if (addr == NULL) return 0; return addr[0]; }

int hpet_write_lo(uint32_t *addr, uint32_t val)
{
    if (addr == NULL)
        return -EINVAL;
    addr[0] = val;
    return 0;
}

uint32_t hpet_read_hi(uint32_t *addr){ if (addr == NULL) return 0; return addr[1]; }

int hpet_write_hi(uint32_t *addr, uint32_t val)
{
    if (addr == NULL)
        return -EINVAL;
    addr[1] = val;
    return 0;
}

int hpet_write64(uintptr_t addr, uint64_t val)
{
    int err = 0;
    if ((err = hpet_write_lo((uint32_t *)addr, val)))
        return err;
    if ((err = hpet_write_hi((uint32_t *)addr, val >> 32)))
        return err;
    return 0;
}

uint64_t hpet_read64(uintptr_t addr)
{
    if (addr == 0)
        return 0;
    uint64_t data = 0;
    data = hpet_read_hi((uint32_t *)addr);
    data <<= 32;
    data |= hpet_read_lo((uint32_t *)addr);
    return data;
}

uint64_t hpet_read_capID(void)
{
    return hpet_read64(HPET_CAPID_REG);
}

int hpet_enable(void);
int hpet_disable(void);
uint32_t hpet_getcounter(uint32_t freq);
int hpet_setup_timer(uint8_t timer, uint64_t comp_val, uint8_t mode, uint8_t type, uint8_t trigger_mode, uint8_t int_route, uint8_t cpu);

int hpet_enumerate(void)
{
    int err = 0;
    void *hdr = NULL;
    if ((err = acpi_enumarate("HPET", (void *)&hdr)))
        return err;
    acpi_hpet = hdr;
    err = acpi_validate_table(hdr, acpi_hpet->HDR.length);
    hpet = (uintptr_t)acpi_hpet->base_addr.timer_block_addr;

    capID_t ID;
    ID.raw = hpet_read64(HPET_CAPID_REG);
    
    hpet_clk = ID.clock_period;
    ntimer = ID.timer_count + 1;
    // int p = acpi_hpet->min_clk_tick / ID.clock_period;

    hpet_setup_timer(0, 1000000, 0, 1, HPET_LEVEL_TRIGGERED, IRQ_HPET, 0);
    hpet_enable();
    return 0;
}

int hpet_enable(void)
{
    int prev_state = 0;
    conf_t conf;
    conf.raw = hpet_read64(HPET_CONF_REG);
    prev_state = conf.enable;
    conf.enable = 1;
    hpet_write64(HPET_CONF_REG, conf.raw);
    return prev_state;
}

int hpet_disable(void)
{
    int prev_state = 0;
    conf_t conf;
    conf.raw = hpet_read64(HPET_CONF_REG);
    prev_state = conf.enable;
    conf.enable = 0;
    hpet_write64(HPET_CONF_REG, conf.raw);
    return prev_state;
}

int hpet_set_upcounter(uint64_t count)
{
    int err = 0;
    int old = hpet_disable();
    if ((err = hpet_write64(HPET_COUNTER_REG, count))){
        if (old)
            hpet_enable();
        return err;
    }
    if (old)
        hpet_enable();
    return 0;
}

int hpet_setup_timer(timer, time, mode, type, trigger_mode, int_route, cpu)
    uint8_t timer; /*which of the timers?*/
    uint64_t time; /*comapare value*/
    uint8_t mode; /*32 or 64 -bit*/
    uint8_t type; /*periodic or non-periodic*/
    uint8_t trigger_mode; /*level or edge -triggered*/
    uint8_t int_route; /*which IOxAPIC interrupt?*/
    uint8_t cpu;
{
    int err = 0;
    int old = 0;
    timer_conf_t confg = {0};

    old = hpet_disable();
    confg.raw = hpet_read64(HPET_TIMER_CONF_REG(timer));
    confg.timer_type = (type ? (confg.periodic_cap ? 1 : 0) /*if capable set periodic mode*/ : 0 /* else one-shot mode*/);
    confg.int_enable = 1; /*enable the timer*/
    if ((err = hpet_write64(HPET_TIMER_CONF_REG(timer), confg.raw)))
    {
        if (old) hpet_enable();
        return err;
    }

    confg.raw = hpet_read64(HPET_TIMER_CONF_REG(timer));
    confg.value_set = (confg.timer_type ? 1 /*set compare value if in periodic mode*/: 0 /*ignore if non-periodic mode*/);
    if ((err = hpet_write64(HPET_TIMER_CONF_REG(timer), confg.raw)))
    {
        if (old) hpet_enable();
        return err;
    }
    
    if ((err = hpet_write64(HPET_TIMER_COMP_VAL_REG(timer), time)))
    {
        if (old) hpet_enable();
        return err;
    }

    /*if ((err = hpet_write64(HPET_TIMER_COMP_VAL_REG(timer), time)))
    {
        if (old) hpet_enable();
        return err;
    }*/
    
    confg.raw = hpet_read64(HPET_TIMER_CONF_REG(timer));
    confg._32mode = !mode ? 1 /*force 32-bit mode*/: 0 /*64-bit mode*/;
    confg.int_route = int_route & 0x1F; /*which IOxAPIC interrupt to use*/
    confg.trigger_mode = trigger_mode ? 1 /*level-triggered*/ : 0 /*edge-triggered*/;
    if ((err = hpet_write64(HPET_TIMER_CONF_REG(timer), confg.raw)))
    {
        if (old) hpet_enable();
        return err;
    }

    active_timers[timer].active = 1;
    active_timers[timer].type = type;
    active_timers[timer]._32mode = confg._32mode;
    active_timers[timer].trigger_mode = trigger_mode;

    if (old) hpet_enable();

    ioapic_enable(confg.int_route, cpu);
    return 0;
}

uint32_t hpet_getcounter(uint32_t freq)
{
    return (hpet_clk / freq);
}

void hpet_intr(void)
{
    uint32_t intsts = hpet_read_lo((uint32_t *)HPET_INT_STS_REG);
    for (int timer = 0; timer < 32; timer++)
    {
        if (active_timers[timer].active)
        {
            if (active_timers[timer].trigger_mode == HPET_LEVEL_TRIGGERED)
            {
                if (CHECK_FLG(intsts, timer))
                    intsts |= _BS(timer);
                hpet_write_lo((uint32_t *)HPET_INT_STS_REG, intsts);
            }

            if (timer == 0)
                jiffies_update();
        }
    }
    //printk("%d\n", jiffies_get());
}