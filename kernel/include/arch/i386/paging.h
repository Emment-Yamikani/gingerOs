#pragma once

#include <arch/i386/32bit.h>
#include <lib/stdint.h>
#include <lib/stddef.h>
#include <mm/mmap.h>

int paging_init(void);
extern void paging_invlpg(uintptr_t);

//TLB flush
void tlb_flush(void);

//unmap page at 'vaddr'
int paging_unmap(uintptr_t vaddr);

//swtich to kernel's page directory
void paging_switchkvm(void);

/**
 * @brief switch to page directory pgdir.
 * if pgdir == 0, then current page directory is used
 * 
 * @param pgdir
 * @return void
 */
uintptr_t paging_switch(uintptr_t pgdir);

//map vaddr to paddr
int paging_map(uintptr_t paddr, uintptr_t vaddr, int flags);
int paging_map_err(uintptr_t frame, uintptr_t v, int flags);

int paging_mappages_err(uintptr_t v, size_t sz, int flags);

void paging_proc_unmap(uintptr_t pgd);

uintptr_t paging_mount(uintptr_t paddr);

void paging_unmount(uintptr_t v);

int paging_memcpypp(uintptr_t dst, uintptr_t src, size_t size);
int paging_memcpyvp(uintptr_t p, uintptr_t v, size_t);
int paging_memcpypv(uintptr_t v, uintptr_t p, size_t);

// map n pages starting at 'vaddr', where n=(sz/PAGESZ)
int paging_mappages(uintptr_t vaddr, size_t sz, int flags);

int paging_unmap_table(int t);

int paging_unmap_mapped(uintptr_t p, size_t sz);

// unmap pages
int paging_unmappages(uintptr_t page, size_t sz);

/// @brief get page mapping if both pagetable and page are mapped in
/// @param vaddr 
/// @return page_t * or NULL if pagetable or page isn't mapped in
page_t *paging_getmapping(uintptr_t vaddr);

// map n pages to frames starting at 'vaddr' and 'paddr', where n=(sz/PAGESZ)
int paging_identity_map(uintptr_t paddr, uint32_t vaddr, size_t sz, int flags);

int paging_mapvmr(const vmr_t *vmr);

uintptr_t paging_alloc(size_t sz);

void paging_free(uintptr_t addr, size_t sz);

uintptr_t paging_getpgdir(void);

int paging_lazycopy(uintptr_t dst, uintptr_t src);