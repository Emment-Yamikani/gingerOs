#ifndef MMU_H
#define MMU_H

#include <lib/stdint.h>
#include <sys/system.h>

#define SEG_NULL 0
#define SEG_KCODE 1 // kernel code
#define SEG_KDATA 2 // kernel data+stack
#define SEG_UCODE 3
#define SEG_UDATA 4
#define SEG_TSS   5
#define SEG_KCPU  6 // per-cpu kernel structure

#define DPL_USER 0x3 // User DPL
#define DPL_KERN 0x0 // Kernel DPL

#define FL_IF   0x200

#define NSEG 7
#define NIDT 256

typedef struct systable
{
    uint16_t lim;
    uint32_t base;
} __packed systable_t;

typedef struct
{
    uint16_t limit0 : 16;
    uint32_t base0 : 24;
    uint32_t access : 8;
    uint16_t limit1 : 4;
    uint16_t flags : 4;
    uint16_t base1 : 8;
} __packed seg_desc_t;

#define SEG(_flags, _access, base, limit)   \
    (seg_desc_t)                            \
    {                                       \
        .limit0 = ((limit)&0xffff),         \
        .base0 = ((base)&0xffffff),         \
        .access = (_access),                \
        .limit1 = (((limit) >> 16) & 0x0f), \
        .flags = ((_flags)&0x0f),           \
        .base1 = (((base) >> 24) & 0xff),   \
    }


typedef struct
{
    uint16_t off0;
    uint16_t sel;
    uint16_t _zero0 : 8;
    uint16_t type : 4;
    uint16_t _zero1 : 1;
    uint16_t dpl : 2;
    uint16_t p : 1;
    uint16_t off1;
} __packed gate_desc_t;

#define SETGATE(istrap, sel, off, dpl)(gate_desc_t)\
{                                             \
    .off0 = ((uint32_t)off) & 0xffff,         \
    .sel = sel & 0xffff,                      \
    ._zero0 = 0,                              \
    .type = (istrap) ? 0x0f : 0x0E,           \
    ._zero1 = 0,                              \
    .dpl = dpl & 0x3,                         \
    .p = 1,                                   \
    .off1 = (((uint32_t)off) >> 16) & 0xffff, \
}

// Task state segment format
typedef struct tss
{
    uint32_t link; // Old ts selector
    uint32_t esp0; // Stack pointers and segment selectors
    uint16_t ss0;  //   after an increase in privilege level
    uint16_t padding1;
    uint32_t *esp1;
    uint16_t ss1;
    uint16_t padding2;
    uint32_t *esp2;
    uint16_t ss2;
    uint16_t padding3;
    void *cr3;     // Page directory base
    uint32_t *eip; // Saved state from last task switch
    uint32_t eflags;
    uint32_t eax; // More saved state (registers)
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t *esp;
    uint32_t *ebp;
    uint32_t esi;
    uint32_t edi;
    uint16_t es; // Even more saved state (segment selectors)
    uint16_t padding4;
    uint16_t cs;
    uint16_t padding5;
    uint16_t ss;
    uint16_t padding6;
    uint16_t ds;
    uint16_t padding7;
    uint16_t fs;
    uint16_t padding8;
    uint16_t gs;
    uint16_t padding9;
    uint16_t ldt;
    uint16_t padding10;
    uint16_t t;    // Trap on task switch
    uint16_t iomb; // I/O map base address
} __packed tss_t;

#endif //MMU_H