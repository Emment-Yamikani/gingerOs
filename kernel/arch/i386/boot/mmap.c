#include <arch/boot/boot.h>
#include <sys/system.h>
#include <lib/string.h>
#include <bits/errno.h>
#include <printk.h>

bootinfo_t bootinfo;

static int get_mmap(multiboot_info_t *info)
{
    multiboot_memory_map_t *mmap = (multiboot_memory_map_t *)info->mmap_addr;
    if (!mmap)
        return -EINVAL;
    if (!(info->flags & MULTIBOOT_INFO_MEM_MAP))
        return -1;

    for (int i =0; mmap < (multiboot_memory_map_t *)(info->mmap_addr + info->mmap_length); ++i)
    {
        bootinfo.mmap[i].addr = mmap->addr;
        bootinfo.mmap[i].size = mmap->len;
        bootinfo.mmap[i].type = mmap->type;
        bootinfo.mmap_count++;
        bootinfo.pmemsize += mmap->len / 1024;
        mmap = (multiboot_memory_map_t *)((uintptr_t)mmap+ mmap->size + sizeof (mmap->size));
    }

    bootinfo.pmemsize &= ~0x3ff; //absolute size 
    return 0;
}

static int get_modules(multiboot_info_t *info)
{
    if (!(info->flags & MULTIBOOT_INFO_MODS))
        return -1;
    multiboot_module_t *mods = (multiboot_module_t *)info->mods_addr;
    for (int i=0; i < (int)info->mods_count; ++i)
    {
        bootinfo.mods[i].addr = VMA_HIGH(mods[i].mod_start);
        bootinfo.mods[i].cmdline = (char *)VMA_HIGH(mods[i].cmdline);
        bootinfo.mods[i].size = mods[i].mod_end - mods[i].mod_start;
        bootinfo.mods_count++;
    }
    return 0;
}

int process_multiboot_info(multiboot_uint32_t flags, multiboot_info_t *info)
{
    int err = 0;
    if (flags != MULTIBOOT_BOOTLOADER_MAGIC)
        return -EINVAL;
    memset(&bootinfo, 0, sizeof (bootinfo));
    bootinfo.mbi = *info;
    if ((err = get_mmap(info)))
        return err;
    if ((err = get_modules(info)))
        return err;
    return 0;
}