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
#include <mm/shm.h>
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

int binfmt_elf_load(struct inode *file, proc_t *proc)
{
    int err =0;
    void *buf = NULL;
    uintptr_t brk =0;
    size_t retval =0;
    vmr_t *vmr = NULL;
    Elf32_Phdr *phdr = NULL;
    Elf32_Ehdr *elf_hdr = NULL;

    if ((buf = kmalloc(file->i_size)) == NULL)
        return -ENOMEM;

    // thisssss IISSSS SOOOO SLOWWWWWWWW!!!!!
    retval = iread(file, 0, (char *)buf, file->i_size);

    if (retval != file->i_size)
    {
        err = -EAGAIN;
        goto error;
    }

    elf_hdr = (typeof (elf_hdr))buf;
    phdr = (typeof (phdr))(buf + elf_hdr->e_phoff);

    for (int i =0; i < elf_hdr->e_phnum; ++i)
    {
        size_t truncate = 0;

        if (phdr[i].p_type == PT_LOAD)
        {
            truncate = phdr[i].p_memsz - phdr[i].p_filesz;
            
            if (vmr_alloc(&vmr))
                panic("binfmt_elf_load(): failed to alloc vmr\n");

            vmr->file = file;
            vmr->vaddr = phdr[i].p_vaddr;
            vmr->size = phdr[i].p_memsz;
            vmr->file_pos = phdr[i].p_offset;
            
            vmr->vflags |= phdr[i].p_flags & PF_X ? VM_UR : 0;
            vmr->vflags |= phdr[i].p_flags & PF_R ? VM_UR : 0;
            vmr->vflags |= phdr[i].p_flags & PF_W ? VM_UW : 0;

            vmr->oflags |= phdr[i].p_flags & PF_R ? VM_READ : 0;
            vmr->oflags |= phdr[i].p_flags & PF_W ? VM_WRITE : 0;
            vmr->oflags |= phdr[i].p_flags & PF_X ? VM_EXECUTABLE : 0;
            vmr->oflags |= phdr[i].p_flags & PF_X ? VM_DONTEXPAND : 0;

            //printk("elf: vmr: %p, oflags: \e[0;013m0%b\e[0m, vflags: \e[0;012m0%b\e[0m\n", vmr->vaddr, vmr->oflags, vmr->vflags);

            if (phdr[i].p_flags & PF_X)
            {
                proc->mmap->code_start = phdr[i].p_vaddr;
                proc->mmap->code_size = phdr[i].p_memsz;
            }
            else
            {
                proc->mmap->data_start = phdr[i].p_vaddr;
                proc->mmap->data_size = phdr[i].p_memsz;
            }
            
            shm_map(proc->mmap, vmr);
            paging_mappages(vmr->vaddr, GET_BOUNDARY_SIZE(vmr->vaddr, vmr->size), vmr->vflags);
            //!TODO: implement demand paging
            //klog(KLOG_WARN, "%s:%d:: [TODO] implement demand paging\n", __FILE__, __LINE__);
            memcpy((void *)phdr[i].p_vaddr, (void *)(buf + phdr[i].p_offset), phdr[i].p_filesz);
            if (truncate)
                memset((void *)(phdr[i].p_vaddr + phdr[i].p_filesz), 0, truncate);
        }

        if (phdr[i].p_vaddr > brk)
        {
            brk = phdr[i].p_vaddr + phdr[i].p_memsz;
            proc->mmap->brk_size -= GET_BOUNDARY_SIZE(0, phdr[i].p_memsz);
        }
    }

    proc->entry = elf_hdr->e_entry;
    proc->mmap->brk_start = GET_BOUNDARY_SIZE(0, brk);
    kfree(buf);
    return 0;
error:
    if (buf) kfree(buf);
    printk("binfmt_elf_load: err=%d\n", err);
    return err;
}