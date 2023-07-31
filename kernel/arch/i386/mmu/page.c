#include <printk.h>
#include <arch/i386/32bit.h>
#include <bits/errno.h>
#include <arch/system.h>
#include <lib/types.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <arch/i386/paging.h>
#include <lib/string.h>
#include <arch/i386/traps.h>
#include <mm/mm_zone.h>

/*flush the tlb*/
void tlb_flush(void)
{
    write_cr3(read_cr3());
}

/*map pagetable*/
static int _32bit_maptable(int t, int flags)
{
    uintptr_t addr = 0;
    if (!(addr = pmman.alloc()))
        return -ENOMEM;
    if (flags & VM_U) // user pagetable always marked writtable, but pages may or may not be writtable
        PDE(t)->raw = PGROUND(addr) | PGOFFSET(flags | VM_PWT | VM_UW);
    else
        PDE(t)->raw = PGROUND(addr) | PGOFFSET(flags | VM_PWT);
    if (t >= 768)
        send_tlb_shootdown();
    paging_invlpg((uintptr_t)PTE(t, 0));
    for (int i = 0; i < 1024; ++i)
        PTE(t, i)->raw = 0;
    paging_invlpg((uintptr_t)PTE(t, 0));
    return 0;
}

/*unmap pagetable*/
static int _32bit_unmaptable(int t)
{
    VM_CHECKBOUNDS(t, 0);
    if (!PDE(t)->structure.p)
        panic("pagetable[%p] unavailable\n", _VADDR(t, 0));
    pmman.free(PGROUND(PDE(t)->raw));
    PDE(t)->raw = 0;
    paging_invlpg((uintptr_t)PTE(t, 0));
    return 0;
}

/*map page*/
static int _32bit_map(uintptr_t frame, int t, int p, int flags)
{
    int err = 0;
    assert(!(frame & PAGEMASK), "frame address must be page aligned");
    VM_CHECKBOUNDS(t, p);
    if (!PDE(t)->structure.p)
    {
        if ((err = _32bit_maptable(t, flags)))
            return err;
    }
    if (PTE(t, p)->structure.p)
        panic("%d:%d page [%p], map:%p available\n", t, p, _VADDR(t, p), paging_getmapping(_VADDR(t, p))->raw);
    PTE(t, p)->raw = PGROUND(frame) | flags;
    paging_invlpg(_VADDR(t, p));
    return 0;
}

static int _32bit_map_err(uintptr_t frame, int t, int p, int flags)
{
    int err = 0;
    assert(!(frame & PAGEMASK), "frame address must be page aligned");
    VM_CHECKBOUNDS(t, p);
    if (!PDE(t)->structure.p)
    {
        if ((err = _32bit_maptable(t, flags)))
            return err;
    }
    if (PTE(t, p)->structure.p)
        return -EEXIST;
    PTE(t, p)->raw = PGROUND(frame) | flags;
    paging_invlpg(_VADDR(t, p));
    return 0;
}

/*unmap page*/
static int _32bit_unmap(int t, int p)
{
    VM_CHECKBOUNDS(t, p);
    if (!PDE(t)->structure.p)
        panic("pagetable[%p] unavailable\n", _VADDR(t, p));
    if (!PTE(t, p)->structure.p)
        panic("page %p unavailable, return [%p]\n", _VADDR(t, p), return_address(0));
    pmman.free(GET_FRAMEADDR(_VADDR(t, p)));
    PTE(t, p)->raw = 0;
    paging_invlpg(_VADDR(t, p));
    return 0;
}

/*unmap page if mapped*/
static int _32bit_unmap_mapped(int t, int p)
{
    VM_CHECKBOUNDS(t, p);
    if (!PDE(t)->structure.p)
    {
        paging_invlpg(_VADDR(t, p));
        return -ENOENT;
    }
    if (!PTE(t, p)->structure.p)
    {
        paging_invlpg(_VADDR(t, p));
        return -ENOENT;
    }
    pmman.free(GET_FRAMEADDR(_VADDR(t, p)));
    PTE(t, p)->raw = 0;
    paging_invlpg(_VADDR(t, p));
    return 0;
}

/*unmap page table*/
int paging_unmap_table(int t)
{
    VM_CHECKBOUNDS(t, 0);
    if (!PDE(t)->structure.p)
        panic("pagetable[%p] unavailable\n", _VADDR(t, 0));
    PDE(t)->raw = 0;
    send_tlb_shootdown();
    paging_invlpg((uintptr_t)PTE(t, 0));
    //tlb_flush();
    return 0;
}

int paging_map(uintptr_t frame, uintptr_t v, int flags)
{
    assert(!(v & PAGEMASK), "page address must be page aligned");
    assert(!(frame & PAGEMASK), "frame address must be page aligned");
    return _32bit_map(frame, V_PDI(v), V_PTI(v), flags);
}

int paging_map_err(uintptr_t frame, uintptr_t v, int flags)
{
    assert(!(v & PAGEMASK), "page address must be page aligned");
    assert(!(frame & PAGEMASK), "frame address must be page aligned");
    return _32bit_map_err(frame, V_PDI(v), V_PTI(v), flags);
}

int paging_unmap_mapped(uintptr_t p, size_t sz)
{
    int err = 0;
    size_t np = (sz / PAGESZ) + (sz % PAGESZ ? 1 : 0);
    while (np--)
    {
        if ((err = _32bit_unmap_mapped(V_PDI(p), V_PTI(p))))
            break;
        p += PAGESZ;
    }
    return err;
}

int paging_unmap(uintptr_t v)
{
    assert(!(v & PAGEMASK), "page address must be page aligned");
    return _32bit_unmap(V_PDI(v), V_PTI(v));
}

int paging_identity_map(uintptr_t frame, uint32_t v, size_t sz, int flags)
{
    int err = 0;
    assert(!(v & PAGEMASK), "page address must be page aligned");
    assert(!(frame & PAGEMASK), "frame address must be page aligned");
    assert(!(sz & PAGEMASK), "invalid size, must be page aligned");
    int np = GET_BOUNDARY_SIZE(v, sz) / PAGESZ;
    while (np--)
    {
        if ((err = paging_map(frame, v, flags)))
            break;
        v += PAGESZ;
        frame += PAGESZ;
    }
    return err;
}

int paging_mappages(uintptr_t v, size_t sz, int flags)
{
    uintptr_t p = 0;
    assert(!(v & PAGEMASK), "page address must be page aligned");
    assert(!(sz & PAGEMASK), "invalid size, must be page aligned");
    int err = 0, np = GET_BOUNDARY_SIZE(v, sz) / PAGESZ;
    while (np--)
    {
        if (!(p = pmman.alloc()))
            return -ENOMEM;
        if ((err = paging_map(p, v, flags)))
            break;
        v += PAGESZ;
    }
    return err;
}

int paging_mappages_err(uintptr_t v, size_t sz, int flags)
{
    uintptr_t p = 0;
    assert(!(v & PAGEMASK), "page address must be page aligned");
    assert(!(sz & PAGEMASK), "invalid size, must be page aligned");
    int err = 0, np = GET_BOUNDARY_SIZE(v, sz) / PAGESZ;
    while (np--)
    {
        if (!(p = pmman.alloc()))
            return -ENOMEM;
        if ((err = paging_map_err(p, v, flags)))
            break;
        v += PAGESZ;
    }
    return err;
}

int paging_unmappages(uintptr_t v, size_t sz)
{
    assert(!(v & PAGEMASK), "page address must be page aligned");
    assert(!(sz & PAGEMASK), "invalid size, must be page aligned");
    int err = 0, np = GET_BOUNDARY_SIZE(v, sz) / PAGESZ;
    //printk("%s:%d: return [%p]\n", __FILE__, __LINE__, return_address(0));
    while (np--)
    {
        if ((err = paging_unmap(v)))
            break;
        v += PAGESZ;
    }
    return err;
}

void paging_proc_unmap(uintptr_t pgd)
{
    uintptr_t oldpgdir = paging_switch(pgd);

    for (int pdi = 0; pdi < 768; ++pdi)
    {
        if (PDE(pdi)->structure.p)
        {
            for (int pte = 0; pte < 1024; ++pte)
            {
                if (PTE(pdi, pte)->structure.p)
                    _32bit_unmap(pdi, pte);
            }
            _32bit_unmaptable(pdi);
        }
    }
    paging_switch(oldpgdir);
}

pte_t *paging_getmapping(uintptr_t v)
{
    pte_t *page = NULL;
    if (!PDE(V_PDI(v))->structure.p)
        return NULL;
    else if (!((page = PTE(V_PDI(v), V_PTI(v)))->structure.p))
        return NULL;
    return page;
}

int paging_mapvmr(const vmr_t *vmr)
{
    if (vmr->paddr)
        return paging_identity_map(vmr->paddr, vmr->start, __vmr_size(vmr), vmr->vflags);
    else
        return paging_mappages(vmr->start, __vmr_size(vmr), vmr->vflags);
}

uintptr_t paging_alloc(size_t sz)
{
    assert(!(sz & PAGEMASK), "invalid size, must be page aligned");
    uintptr_t base = vmman.alloc(sz);
    if (!base) {
        printk("%s:%d: base(%p) error, retaddr: %p\n", __FILE__, __LINE__, base, return_address(0));
        return 0;
    }
    else if (paging_mappages(base, sz, VM_KRW)) {
        printk("%s:%d: error\n", __FILE__, __LINE__);
        return 0;
    }
    return base;
}

void paging_free(uintptr_t v, size_t sz)
{
    assert(!(v & PAGEMASK), "page address must be page aligned");
    assert(!(sz & PAGEMASK), "invalid size, must be page aligned");
    paging_unmappages((uintptr_t)v, sz);
    vmman.free(v);
}

uintptr_t paging_mount(uintptr_t p)
{
    int err = 0;
    uintptr_t v = 0;
    assert(p, "no paddr");
    assert(!(p & PAGEMASK), "frame address must be page aligned");
    v = vmman.alloc(PAGESZ);
    if ((err = paging_map(p, v, (VM_KRW | VM_PWT | VM_PCD))))
    {
        vmman.free(v);
        klog(KLOG_FAIL, "failed to mount paddr, error: %d\n", err);
        return 0;
    }
    return v;
}

void paging_unmount(uintptr_t v)
{
    assert(!(v & PAGEMASK), "page address must be page aligned");
    if (!PDE(V_PDI(v))->structure.p)
        panic("%s:%d: pagetable[%p] unavailable\n", __FILE__, __LINE__, _VADDR(V_PDI(v), V_PTI(v)));
    if (!PTE(V_PDI(v), V_PTI(v))->structure.p)
        panic("%s:%d: page %p unavailable\n", __FILE__, __LINE__, _VADDR(V_PDI(v), V_PTI(v)));
    PTE(V_PDI(v), V_PTI(v))->raw = 0;
    paging_invlpg(v);
    vmman.free(v);
}

uintptr_t paging_getpgdir(void)
{
    uint32_t p = pmman.alloc(), *v = NULL, *cur_pgdir = NULL;
    if (!(v = (uint32_t *)paging_mount(p)))
    {
        pmman.free(p);
        return 0;
    }
    if (!(cur_pgdir = (uint32_t *)paging_mount(PGROUND(read_cr3()))))
    {
        paging_unmount((uintptr_t)v);
        return 0;
    }
    for (int i = 0; i < 1024; ++i)
        v[i] = 0;
    for (int i = 768; i < 1023; ++i)
        v[i] = cur_pgdir[i];
    v[1023] = (p | VM_KRW | VM_PCD | VM_PWT);
    paging_unmount((uintptr_t)cur_pgdir);
    paging_unmount((uintptr_t)v);
    return p;
}

int paging_memcpypp(uintptr_t pdst, uintptr_t psrc, size_t size)
{
    uintptr_t vdst = 0, vsrc = 0;
    size_t len = 0;
    
    while (size)
    {
        if ((vdst = paging_mount(PGROUND(pdst))) == 0)
            return -ENOMEM;

        if ((vsrc = paging_mount(PGROUND(psrc))) == 0)
        {
            paging_unmount(vdst);
            return -ENOMEM;
        }

        len = MIN(PAGESZ - MAX(PGOFFSET(psrc), PGOFFSET(pdst)), PAGESZ);
        len = MIN(len, size);
        memcpy((void *)(vdst + PGOFFSET(pdst)), (void *)(vsrc + PGOFFSET(psrc)), len);
        size -= len;
        psrc += len;
        pdst += len;
        paging_unmount(vsrc);
        paging_unmount(vdst);
    }

    return 0;
}

int paging_memcpyvp(uintptr_t p, uintptr_t v, size_t size)
{
    uintptr_t vdst = 0;
    size_t len = 0;
    while (size)
    {
        if ((vdst = paging_mount(PGROUND(p))) == 0)
            return -ENOMEM;

        len = MIN(PAGESZ - PGOFFSET(p), size);
        memcpy((void *)(vdst + PGOFFSET(p)), (void *)v, len);
        size -= len;
        p += len;
        v += len;
        paging_unmount(vdst);
    }

    return 0;
}

int paging_memcpypv(uintptr_t v, uintptr_t p, size_t size)
{
    uintptr_t vsrc = 0;
    size_t len = 0;
    while (size)
    {
        if ((vsrc = paging_mount(PGROUND(p))) == 0)
            return -ENOMEM;

        len = MIN(PAGESZ - PGOFFSET(p), size);
        memcpy((void *)v, (void *)(vsrc + PGOFFSET(p)), len);
        size -= len;
        p += len;
        v += len;
        paging_unmount(vsrc);
    }

    return 0;
}

int paging_init(void)
{
    for (int pdi = 772; pdi < V_PDI(MMAP_DEVADDR); pdi++)
        if (!PDE(pdi)->structure.p)
            _32bit_maptable(pdi, VM_KRW | VM_PCD | VM_PWT);
    return 0;
}

int paging_lazycopy(uintptr_t dst, uintptr_t src)
{
    int err = 0;
    pte_t *srcpt = NULL;
    pde_t *srcpd = NULL;
    uintptr_t oldpgdir = 0;

    if (!(srcpd = __cast_to_type(srcpd) paging_mount(src)))
    {
        err = -ENOMEM;
        goto error;
    }

    oldpgdir = paging_switch(dst);

    for (int i = 0; i < 768; ++i)
    {
        if (!srcpd[i].structure.p)
            continue;

        if ((err = _32bit_maptable(i, srcpd[i].raw & PAGEMASK)))
            goto error;

        if ((srcpt = __cast_to_type(srcpt) paging_mount((uintptr_t)PGROUND(srcpd[i].raw))) == NULL)
        {
            err = -ENOMEM;
            goto error;
        }

        for (int j =0; j < 1024; ++j)
        {
            if (!srcpt[j].structure.p)
                continue;
            PTE(i, j)->raw = srcpt[j].raw & ~VM_W;
            if (srcpt[j].structure.w)
                srcpt[j].structure.w = 0;
            send_tlb_shootdown();
            __page_incr(PGROUND(srcpt[j].raw));
        }

        paging_unmount((uintptr_t)srcpt);
    }

    paging_switch(oldpgdir);
    paging_unmount((uintptr_t)srcpd);
    
    return 0;
error:
    return err;
}