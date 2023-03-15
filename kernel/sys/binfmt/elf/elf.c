/*
 *          ELF format loader
 *
 *
 *  This file is part of Aquila OS and is released under
 *  the terms of GNU GPLv3 - See LICENSE.
 *
 *  Copyright (C) 2016 Mohamed Anwar <mohamed_anwar@opmbx.org>
 */

#include <fs/fs.h>
#include <sys/proc.h>
#include <mm/kalloc.h>
#include <bits/errno.h>
#include <lib/string.h>
#include <arch/i386/paging.h>
#include <sys/elf/elf.h>
#include <mm/mmap.h>
#include <printk.h>

static int elf_check_file(Elf32_Ehdr *elf_hdr)
{
    if (!elf_hdr)
        return 0;
    if (elf_hdr->e_ident[EI_MAG0] != ELFMAG0)
    {
        printk("ELF Header EI_MAG0 incorrect.\n");
        return 0;
    }
    if (elf_hdr->e_ident[EI_MAG1] != ELFMAG1)
    {
        printk("ELF Header EI_MAG1 incorrect.\n");
        return 0;
    }
    if (elf_hdr->e_ident[EI_MAG2] != ELFMAG2)
    {
        printk("ELF Header EI_MAG2 incorrect.\n");
        return 0;
    }
    if (elf_hdr->e_ident[EI_MAG3] != ELFMAG3)
    {
        printk("ELF Header EI_MAG3 incorrect.\n");
        return 0;
    }
    return 1;
}

static int elf_check_supported(Elf32_Ehdr *elf_hdr)
{
    if (!elf_check_file(elf_hdr))
    {
        printk("Invalid ELF File.\n");
        return 0;
    }
    if (elf_hdr->e_ident[EI_CLASS] != ELFCLASS32)
    {
        printk("Unsupported ELF File Class.\n");
        return 0;
    }
    if (elf_hdr->e_ident[EI_DATA] != ELFDATA2LSB)
    {
        printk("Unsupported ELF File byte order.\n");
        return 0;
    }
    if (elf_hdr->e_machine != EM_386)
    {
        printk("Unsupported ELF File target.\n");
        return 0;
    }
    if (elf_hdr->e_ident[EI_VERSION] != EV_CURRENT)
    {
        printk("Unsupported ELF File version.\n");
        return 0;
    }
    if (elf_hdr->e_type != ET_REL && elf_hdr->e_type != ET_EXEC)
    {
        printk("Unsupported ELF File type.\n");
        return 0;
    }
    return 1;
}

int binfmt_elf_check(struct inode *file)
{
    Elf32_Ehdr elf_hdr;
    iread(file, 0, (char *)&elf_hdr, sizeof(elf_hdr));
    /* Check header */
    if (!elf_check_file(&elf_hdr))
        return -ENOEXEC;
    /* check if we support this file */
    if (!elf_check_supported(&elf_hdr))
        return -ENOEXEC;
    return 0;
}

static int elf_get_symval(Elf32_Ehdr *hdr, int table, uint32_t idx)
{
    if (table == SHN_UNDEF || idx == SHN_UNDEF)
        return 0;
    Elf32_Shdr *symtab = elf_section(hdr, table);
    uint32_t symtab_entries = symtab->sh_size / symtab->sh_entsize;
    if (idx >= symtab_entries)
    {
        printk("Symbol Index out of Range (%d:%u).\n", table, idx);
        return ELF_RELOC_ERR;
    }
    int symaddr = (int)hdr + symtab->sh_offset;
    Elf32_Sym *symbol = &((Elf32_Sym *)symaddr)[idx];
    if (symbol->st_shndx == SHN_UNDEF)
    {
        // External symbol, lookup value
        Elf32_Shdr *strtab = elf_section(hdr, symtab->sh_link);
        const char *name = (const char *)hdr + strtab->sh_offset + symbol->st_name;
        extern void *elf_lookup_symbol(const char *name);
        void *target = elf_lookup_symbol(name);
        if (target == NULL)
        {
            // Extern symbol not found
            if (ELF32_ST_BIND(symbol->st_info) & STB_WEAK)
            {
                // Weak symbol initialized as 0
                return 0;
            }
            else
            {
                printk("Undefined External Symbol : %s.\n", name);
                return ELF_RELOC_ERR;
            }
        }
        else
        {
            return (int)target;
        }
    }
    else if (symbol->st_shndx == SHN_ABS)
    {
        // Absolute symbol
        return symbol->st_value;
    }
    else
    {
        // Internally defined symbol
        Elf32_Shdr *target = elf_section(hdr, symbol->st_shndx);
        return (int)hdr + symbol->st_value + target->sh_offset;
    }
}

static int elf_do_reloc(Elf32_Ehdr *hdr, Elf32_Rel *rel, Elf32_Shdr *reltab)
{
    Elf32_Shdr *target = elf_section(hdr, reltab->sh_info);
    int addr = (int)hdr + target->sh_offset;
    int *ref = (int *)(addr + rel->r_offset);
    // Symbol value
    int symval = 0;
    if (ELF32_R_SYM(rel->r_info) != SHN_UNDEF)
    {
        symval = elf_get_symval(hdr, reltab->sh_link, ELF32_R_SYM(rel->r_info));
        if (symval == ELF_RELOC_ERR)
            return ELF_RELOC_ERR;
    }
    // Relocate based on type
    switch (ELF32_R_TYPE(rel->r_info))
    {
    case R_386_NONE:
        // No relocation
        break;
    case R_386_32:
        // Symbol + Offset
        *ref = DO_386_32(symval, *ref);
        break;
    case R_386_PC32:
        // Symbol + Offset - Section Offset
        *ref = DO_386_PC32(symval, *ref, (int)ref);
        break;
    default:
        // Relocation type not supported, display error and return
        printk("Unsupported Relocation Type (%d).\n", ELF32_R_TYPE(rel->r_info));
        return ELF_RELOC_ERR;
    }
    return symval;
}

static __unused int elf_load_stage2(Elf32_Ehdr *elf_hdr)
{
    Elf32_Shdr *shdr = elf_sheader(elf_hdr);
    unsigned int i, idx;
    // Iterate over section headers
    for (i = 0; i < elf_hdr->e_shnum; i++)
    {
        Elf32_Shdr *section = &shdr[i];
        // If this is a relocation section
        if (section->sh_type == SHT_REL)
        {
            // Process each entry in the table
            for (idx = 0; idx < section->sh_size / section->sh_entsize; idx++)
            {
                Elf32_Rel *reltab = &((Elf32_Rel *)((int)elf_hdr + section->sh_offset))[idx];
                int result = elf_do_reloc(elf_hdr, reltab, section);
                // On printk, display a message and return
                if (result == ELF_RELOC_ERR)
                {
                    printk("Failed to relocate symbol.\n");
                    return ELF_RELOC_ERR;
                }
            }
        }
    }
    return 0;
}

#define DEMAND_PAGING 1

int binfmt_elf_load(inode_t *binary, proc_t *proc)
{
    int err = 0;
    int prot = 0;
    int flags = 0;
    size_t phdrsz = 0;
    mmap_t *mmap = NULL;
    vmr_t *region = NULL;
    Elf32_Phdr *hdr = NULL;
    Elf32_Ehdr *elf = NULL;

    proc_assert_lock(proc);
    mmap_assert_locked(proc->mmap);

    mmap = proc->mmap;

    if ((elf = kcalloc(1, sizeof *elf)) == NULL)
    {
        err = -ENOMEM;
        goto error;
    }

    if (iread(binary, 0, elf, sizeof *elf) != sizeof *elf)
    {
        err = -EAGAIN;
        goto error;
    }

    phdrsz = elf->e_phentsize * elf->e_phnum;

    if ((hdr = kcalloc(elf->e_phnum, elf->e_phentsize)) == NULL)
    {
        err = -ENOMEM;
        goto error;
    }

    if (iread(binary, elf->e_phoff, hdr, phdrsz) != phdrsz)
    {
        err = -EAGAIN;
        goto error;
    }

    for (int i = 0; i < elf->e_phnum; i++)
    {
        if (hdr[i].p_type & PT_LOAD)
        {
            flags = MAP_PRIVATE | MAP_DONTEXPAND | MAP_FIXED;
            prot = (hdr[i].p_flags & PF_R ? PROT_R : 0) |
                   (hdr[i].p_flags & PF_W ? PROT_W : 0) |
                   (hdr[i].p_flags & PF_X ? PROT_X : 0);

            if ((err = mmap_map_region(mmap, PGROUND(hdr[i].p_vaddr),
                                       GET_BOUNDARY_SIZE(0, hdr[i].p_memsz), prot, flags, &region)))
                goto error;

#ifndef DEMAND_PAGING
            if ((err = paging_mappages(region->start,
                                       GET_BOUNDARY_SIZE(0, hdr[i].p_memsz), region->vflags)))
                goto error;

            size_t truncsz = __vmr_size(region) - hdr[i].p_filesz;
            if (iread(binary, hdr[i].p_offset, (void *)region->start, hdr[i].p_filesz) != hdr[i].p_filesz)
            {
                err = -EAGAIN;
                goto error;
            }
            memset((void *)region->start + hdr[i].p_filesz, 0, truncsz);
#endif // DEMAND_PAGING

            region->file = binary;
            region->filesz = hdr[i].p_filesz;
            region->file_pos = hdr[i].p_offset;
        }
    }

    kfree(elf);
    kfree(hdr);

    proc->entry = elf->e_entry;

    return 0;
error:
    if (elf)
        kfree(elf);
    if (hdr)
        kfree(hdr);
    return err;
}