#include <arch/chipset/mp.h>
#include <acpi/acpi.h>
#include <lib/string.h>
#include <arch/i386/cpu.h>
#include <arch/firmware/bios.h>
#include <arch/system.h>
#include <printk.h>
#include <arch/i386/paging.h>
#include <arch/i386/lapic.h>

volatile int ismp =0;
volatile int disable_pic =0;

int mp_init(void);

int mp_process(void)
{
    //memset(&cpus, 0, sizeof(cpus));
    if ((ismp = acpi_mp()))
        goto disable;
    else if (!(ismp = mp_init()))
        panic("not MP-Compatible");

disable:
    if (disable_pic)
    {
        // Bochs doesn't support IMCR, so this doesn't run on Bochs.
        // But it would on real hardware.
        outb(0x22, 0x70);          // Select IMCR
        outb(0x23, inb(0x23) | 1); // Mask external interrupts.
    }

    return 0;
}


#include <lib/stdint.h>
#include <lib/stddef.h>
#include <lib/stdbool.h>

#define MP_SIGNATURE 0x5f504d5f

#define MP_PROC     0x0
#define MP_BUS      0x1
#define MP_IOAPIC   0x2
#define MP_IOINTR    0x03  // One per bus interrupt source
#define MP_LINTR     0x04  // One per system interrupt source


typedef struct MP_floating_pointer_struct
{
    uint32_t    signature;
    uint32_t    floatingpointer;
    uint8_t     length;
    uint8_t     version;
    uint8_t     checksum;
    uint8_t     mpfeatures1;
    union
    {
        struct{
            uint8_t _0 : 1;
            uint8_t _1 : 1;
            uint8_t _2 : 1;
            uint8_t _3 : 1;
            uint8_t _4 : 1;
            uint8_t _5 : 1;
            uint8_t _6 : 1;
            uint8_t _icmr : 1;
        };
        uint8_t     raw;
    } mpfeatures2;
    uint8_t     mpfeatures[3];
} MP_floating_pointer_struct_t;

typedef struct MP_configtable_t
{
    uint32_t    signature;
    uint16_t    base_table_length;
    uint8_t     specs_rev;
    uint8_t     checksum;
    uint8_t     oem_id[8];
    uint8_t     prod_id[12];
    uint32_t    oem_table_pointer;
    uint16_t    oem_table_size;
    uint16_t    entry_count;
    uint32_t    addr_lapic;
    uint16_t    ext_table_length;
    uint8_t     ext_table_checksum;
} MP_configtable_t;

typedef struct MP_processor
{
    uint8_t     entrytype;
    uint8_t     lapic_id;
    uint8_t     lapic_ver;
    union{
        struct
        {
            uint8_t cpu_enabled: 1;
            uint8_t cpu_bsp: 1;
        };
        uint8_t raw;
    } cpu_flags;

    uint32_t    cpu_signature;
    uint32_t    cpu_features;
    uint32_t    _[2];
} MP_processor_t;


typedef struct MP_ioapic
{
    uint8_t     entrytype;
    uint8_t     ioapic_id;
    uint8_t     ioapic_ver;
    union
    {
        struct{
            uint8_t ioapic_enabled : 1;
        };
        uint8_t raw;
    }ioapic_flags;
    uint32_t    ioapic_addr;
} MP_ioapic_t;

int icmr = 0;
int nbus = 0;
int nioapic = 0;
int nintlines = 0;


char *mpconfigentries = NULL;
MP_floating_pointer_struct_t *mpfps = NULL;
MP_configtable_t *pcmp = NULL;
MP_processor_t *processor = NULL;
MP_ioapic_t *ioapic_struct = NULL;

int mp_init(void)
{
    uint32_t *signature = (uint32_t *)VMA_HIGH(639 * 1024);
    for (; signature <= (uint32_t *)VMA_HIGH(640 * 1024);)
    {
        if (*signature == MP_SIGNATURE)
        {
            mpfps = (MP_floating_pointer_struct_t *)signature;
            ismp = 1;
            break;
        }
        signature++;
    }

    if (!mpfps)
    {
        signature = (uint32_t *)BIOSROM;
        for (; signature <= (uint32_t *)VMA_HIGH(0xfffff);)
        {
            if (*signature == MP_SIGNATURE)
            {
                ismp = 1;
                mpfps = (MP_floating_pointer_struct_t *)signature;
                break;
            }
            signature++;
        }
    }

    if (mpfps)
    {
        if (!mpfps->mpfeatures2._icmr)
            icmr = 1;
        outb(0x22, 0x70);          // Select IMCR
        outb(0x23, inb(0x23) | 1); // Mask external interrupts.
        pcmp = (MP_configtable_t *)VMA_HIGH(mpfps->floatingpointer);
    }
    else
    {
        printk("system not mp-compliant\n");
        return -1;
    }

    if (pcmp)
    {
        mpconfigentries = (char *)&pcmp[1];
        lapic = (uint32_t *)pcmp->addr_lapic;

        if (!paging_getmapping(pcmp->addr_lapic))
            paging_identity_map(pcmp->addr_lapic, pcmp->addr_lapic, PAGESZ, VM_KRW | VM_PCD);

        int ent_cnt = pcmp->entry_count;
        if (pcmp->specs_rev != 1 && pcmp->specs_rev != 4)
            return -1;

        while (--ent_cnt)
        {
            switch (*mpconfigentries)
            {
            case MP_PROC:
                processor = (MP_processor_t *)mpconfigentries;
                if (processor->cpu_flags.cpu_enabled)
                {
                    cpus[processor->lapic_id].cpuid = processor->lapic_id;
                    ncpu++;
                }
                mpconfigentries += sizeof(MP_processor_t);
                break;
            case MP_IOAPIC:
                ioapic_struct = (MP_ioapic_t *)mpconfigentries;
                if (ioapic_struct->ioapic_flags.ioapic_enabled)
                {
                    ioapic = (uint32_t *)ioapic_struct->ioapic_addr;
                    ioapic_id = ioapic_struct->ioapic_id;
                    if (!paging_getmapping((uint32_t)ioapic))
                        paging_identity_map((uint32_t)ioapic, (uint32_t)ioapic, PAGESZ, VM_KRW | VM_PCD);
                    nioapic++;
                }
                mpconfigentries += sizeof(MP_ioapic_t);
                break;
            case MP_BUS:
                nbus++;
                mpconfigentries += 8;
                break;
            case MP_IOINTR:
                mpconfigentries += 8;
                break;
            case MP_LINTR:
                nintlines++;
                mpconfigentries += 8;
                break;
            default:
                panic("Entry type %d\n", *mpconfigentries);
                break;
            }
        }
    }
    if (nioapic > 1)
        panic("ginger OS not ready to handle more than one ioapic, found %d ioapics\n", nioapic);
    return 1;
}