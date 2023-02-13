#include <lib/stdint.h>
#include <arch/firmware/bios.h>
#include <lib/string.h>
#include <acpi/acpi.h>
#include <arch/i386/paging.h>

int acpi_version = 0;
void *ACPI_RSDP_addr = NULL;
// #include <printk.h>

int acpi_validate_table(char *addr, int size)
{
    uint8_t sum = 0;
    while (size--)
        sum += *addr++;
    return sum == 0;
}

void *acpi_findrsdp(void)
{
    char *rsdp;
    if (ACPI_RSDP_addr)
        return ACPI_RSDP_addr;

    for (rsdp = (char *)EBDA; rsdp < (char *)(EBDA + 1024); rsdp +=4)
        if (!strncmp(rsdp, "RSD PTR ", 8))
            return (void *)(ACPI_RSDP_addr = rsdp);
    for (rsdp = (char *)BIOSROM; rsdp < (char *)(BIOSROM + 0xfffff); rsdp +=4)
        if (!strncmp(rsdp, "RSD PTR ", 8))
            return (void *)(ACPI_RSDP_addr = rsdp);
    return 0;
}

acpiSDT_t *acpi_parse_rsdt(acpiSDT_t *rsdt, char *sign)
{
    uint32_t *entry = (uint32_t *)(((char *)rsdt) + sizeof(*rsdt));
    long entries = (rsdt->length - sizeof(*rsdt)) / 4;
    for (int i =0; i < entries; i++)
    {
        acpiSDT_t *sdt = (acpiSDT_t *)entry[i];
        if (!strncmp(sdt->signature, sign, 4))
            return sdt;
    }
    return 0;
}

acpiSDT_t *acpi_parse_xsdt(acpiSDT_t *xsdt, char *sign)
{
    uint64_t *entry = (uint64_t *)(((char *)xsdt) + sizeof(*xsdt));
    long entries = (xsdt->length - sizeof(*xsdt)) / 8;
    for (int i = 0; i < entries; i++)
    {
        acpiSDT_t *sdt = (acpiSDT_t *)((uint32_t)entry[i]);
        if (!strncmp(sdt->signature, sign, 4))
            return (acpiSDT_t *)sdt;
    }
    return 0;
}

acpiMADT_t *acpi_find_madt(rsdp20_t *rsdp, int revno)
{
    acpiMADT_t *madt = 0;
    if (revno < 2)
        madt = (acpiMADT_t *)acpi_parse_rsdt((acpiSDT_t *)rsdp->rsdp.rsdtaddr, "APIC");
    else{
        madt = (acpiMADT_t *)acpi_parse_xsdt((acpiSDT_t *)((uint32_t)rsdp->xsdtaddr), "APIC");
    }
    return madt;
}

#include <arch/i386/cpu.h>
#include <arch/i386/lapic.h>

int acpi_parse_madt(acpiMADT_t *madt)
{
    int ioapics =0;
    if (!madt)
        return 0;
    
    disable_pic = (int)(madt->flags & 1);
    if (!paging_getmapping(madt->lapic_addr))
        paging_identity_map(madt->lapic_addr, madt->lapic_addr, PAGESZ, VM_KRW | VM_PCD);
    lapic = (uint32_t *)madt->lapic_addr;
    char *entry = (char *)madt + sizeof(*madt);
    while(entry < ((char *)madt) + madt->madt.length)
    {
        switch (*entry)
        {
        case ACPI_APIC:
            cpus[(int)entry[3]].cpuid = (int)entry[3];
            cpus[(int)entry[3]].enabled = (int)entry[4];
            ncpu++;
            break;
        case ACPI_IOAPIC:
            ioapic = (volatile uint32_t *)(*((uint32_t *)&entry[4]));
            ioapic_id = (int)entry[2];
            ioapics++;
            if (!paging_getmapping((uint32_t)ioapic))
                paging_identity_map((uint32_t)ioapic, (uint32_t)ioapic, PAGESZ, VM_KRW | VM_PCD);
            break;
        default:
            break;
        }
        entry += entry[1];
    }

/*
    if (ioapics > 1)
        panic("giner OS not ready to handle more than one ioapic, found %d ioapics\n", ioapics);
*/
    return 1;
}

int acpi_mp(void)
{
    rsdp20_t *rsdp = (rsdp20_t *)acpi_findrsdp();
    if (!rsdp)
        return 0;
    int size = (rsdp->rsdp.revno < 2) ? sizeof(rsdp_t) : rsdp->length;
    acpi_validate_table((char *)rsdp, size);
    acpiMADT_t *madt = acpi_find_madt(rsdp, rsdp->rsdp.revno);
    if (!madt)
        return 0;
    return acpi_parse_madt(madt);
}

int acpi_enumarate(const char *signature, acpiSDT_t **ref)
{
    acpiSDT_t *ACPI_HDR = NULL;
    rsdp20_t *rsdp = (rsdp20_t *)acpi_findrsdp();
    if (ref == NULL)
        return -22;
    if (rsdp == NULL)
        return -2;
    int size = (rsdp->rsdp.revno < 2) ? sizeof (rsdp_t) : rsdp->length;
    acpi_validate_table((char *)rsdp, size);
    ACPI_HDR = (acpiSDT_t *)((rsdp->rsdp.revno < 2)
                ? acpi_parse_rsdt((acpiSDT_t *)rsdp->rsdp.rsdtaddr, (char *)signature)
                : acpi_parse_xsdt((acpiSDT_t *)(uintptr_t)rsdp->xsdtaddr, (char *)signature));
    if (ACPI_HDR == NULL)
        return -2;    
    *ref = ACPI_HDR;
    return 0;
}