#ifndef ACPI_H
#define ACPI_H
#include <lib/stdint.h>
#include <sys/system.h>

/*header for a acpi v1.0*/
typedef struct rsdp
{
    char signature[8]; //signature
    uint8_t checksum;
    char oemid[6];
    uint8_t revno;     //revision number
    uint32_t rsdtaddr; //rsdt address
} __packed rsdp_t;

/*header for acpi v2.0*/
typedef struct rsdp20
{
    rsdp_t rsdp;
    uint32_t length;
    uint64_t xsdtaddr;
    uint8_t ext_checksum;
    uint8_t revsd[3];
} __packed rsdp20_t;

typedef struct acpiSDT
{
    char signature[4];
    uint32_t length;
    uint8_t revno; //revision number
    uint8_t checksum;
    char oemid[6];
    char oemtable_id[8];
    uint32_t oemrevno;
    uint32_t creator_id;
    uint32_t creator_revno;
} __packed acpiSDT_t;

typedef struct acpiMADT
{
    acpiSDT_t madt;
    uint32_t lapic_addr;
    uint32_t flags;
} __packed acpiMADT_t;

#define ACPI_APIC   0
#define ACPI_IOAPIC 1

int acpi_mp(void);
void *acpi_findrsdp(void);
int acpi_parse_madt(acpiMADT_t *madt);
acpiMADT_t *acpi_find_madt(rsdp20_t *rsdp, int revno);
int acpi_validate_table(char *addr, int size);
acpiSDT_t *acpi_parse_rsdt(acpiSDT_t *rsdt, char *sign);
acpiSDT_t *acpi_parse_xsdt(acpiSDT_t *xsdt, char *sign);

int acpi_enumarate(const char *signature, acpiSDT_t **ref);

extern int acpi_version;
extern void *ACPI_RSDP_addr;


#endif //ACPI_H