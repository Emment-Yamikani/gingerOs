#ifndef _32BIT_H
#define _32BIT_H

#include <lib/stdint.h>
#include <sys/system.h>
#include <lime/assert.h>

typedef union _page_
{
    struct
    {
        uint32_t p : 1; //present
        uint32_t w : 1; //writtable
        uint32_t u : 1; //user
        uint32_t pwt : 1; //page level write-through
        uint32_t pcd : 1; //page level cache disable
        uint32_t a : 1; //accessed
        uint32_t d : 1; //dirty
        uint32_t pat : 1;
        uint32_t g : 1;
        uint32_t ignore : 3 ;
        uint32_t phy : 20;
    } structure;
    uint32_t raw;
} __packed page_t;

typedef union _table_
{
    struct
    {
        uint32_t p : 1; //present
        uint32_t w : 1; //writtable
        uint32_t u : 1; //user
        uint32_t pwt : 1; //page level write-through
        uint32_t pcd : 1; //page level cache disable
        uint32_t a : 1; //accessed
        uint32_t ign0: 1; //dirty
        uint32_t zero : 1;
        uint32_t ignore : 4 ;
        uint32_t phy : 20;
    } structure;
    uint32_t raw;
} __packed table_t;

typedef union viraddr
{
    struct
    {
        uint32_t off : 12;
        uint32_t pti : 10;
        uint32_t pdi : 10;
    }structure;
    uint32_t raw;
} __packed viraddr_t;

#define VM_P    _BS(0)
#define VM_W    _BS(1)
#define VM_U    _BS(2)
#define VM_PWT  _BS(3)
#define VM_PCD  _BS(4)

#define VM_UX      0x0020  /* User eXecute */
#define VM_FILE    0x0080  /* File backed */
#define VM_ZERO    0x0100  /* Zero fill */
#define VM_KR   VM_P
#define VM_KRW  VM_W | VM_P
#define VM_UR   VM_U | VM_P
#define VM_UW   VM_U | VM_W
#define VM_URW  VM_U | VM_W | VM_P

//all processes have their page-dir mapped-in here
#define PGDIR ((table_t *)0xfffff000)

//base address for all page table in the same address space
#define PAGETBL(i) ((page_t *)(0xffc00000 + (0x1000 * (i))))

//get page directory entry(t)
#define PDE(t) (&PGDIR[(t)])

//get page table entry(p)
#define PTE(t, p) (&PAGETBL((t))[(p)])

#define VM_CHECKBOUNDS(t, p)\
assert((!((t) > 1023) || ((p) > 1023)), "error page out of bounds\n");

//virtual address
#define _VADDR(t, p) \
    ((viraddr_t) {.structure.pdi = (t), .structure.pti = (p)}.raw)

#define V_PTI(v) ((viraddr_t){.raw = (v)}.structure.pti)
#define V_PDI(v) ((viraddr_t){.raw = (v)}.structure.pdi)

#define GET_FRAMEADDR(v) ((uintptr_t)PGROUND(PTE(V_PDI(v), V_PTI(v))->raw))
#endif //_32BIT_H