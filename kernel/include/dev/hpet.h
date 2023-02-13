#ifndef DEV_HPET_H
#define DEV_HPET_H 1

#include <sys/system.h>
#include <lib/stdint.h>

// General Capabilities and ID register
typedef union capID
{
    struct
    {
        uint8_t revision_ID;
        uint8_t timer_count: 5;
        uint8_t counter_size: 1;
        uint8_t resvd: 1;
        uint8_t legacy_cap: 1;
        uint16_t vendor_id;
        uint32_t clock_period;
    }__packed;
    uint64_t raw;
}__packed capID_t;

typedef union conf
{
    struct
    {
        uint8_t enable: 1;
        uint8_t legacy_route: 1;
        uint8_t resvd0 : 5;
        uint8_t resvd1;
        uint64_t resvd2: 48;
    }__packed;
    uint64_t raw;
} __packed conf_t;

typedef union timer_conf
{
    struct
    {
        uint8_t resvd0 : 1;
        uint8_t trigger_mode : 1;
        uint8_t int_enable : 1;
        uint8_t timer_type : 1;
        uint8_t periodic_cap: 1; // read-only
        uint8_t timer_size: 1; // read-only
        uint8_t value_set: 1; // read-write
        uint8_t resvd1: 1;
        uint8_t _32mode: 1; // read-write
        uint8_t int_route: 5; 
        uint8_t fsb_enable: 1;
        uint8_t fsb_int_delivery: 1;
        uint16_t resvd2;
        uint32_t int_route_cap;
    } __packed;
    uint64_t raw;
} __packed timer_conf_t;

typedef union
{
    struct
    {
        uint32_t fsb_msg;
        uint32_t fsb_addr;
    } __packed;
    uint64_t raw;
}__packed fsb_route_t;

typedef struct hpet_timer
{
    timer_conf_t conf_cap; // configurations & capabilities register
    uint64_t comp_val; // comparator value register
    fsb_route_t fsb_rt; // front side bus interrupt route register
} hpet_timer_t;

typedef struct hpet_timerblock
{
    capID_t capID; // general capabilities and ID register
    uint64_t rsvd0;
    conf_t conf; //general configurations register
    uint64_t rsvd1;
    uint64_t int_sts; // general interrupt status register
    uint64_t rsvd2[25];
    uint64_t counter; // main counter value register
    uint64_t rsvd3;
    hpet_timer_t timer[32]; // array of timers on this HPET timer block
} __packed hpet_timerblock_t;

int hpet_enumerate(void);
void hpet_intr(void);

#endif // DEV_HPET_H