#ifndef __BOOT__
#define __BOOT__
#include "multiboot.h"
#include <lib/stdint.h>
#include <lib/stddef.h>
#include <lib/types.h>
#include <dev/fb.h>

typedef unsigned long long u64_t;

typedef struct bootinfo
{
    int     mmap_count;
    int     mods_count;
    u64_t  pmemsize;   //memory size in Kb
    char    *kcmdline;
    //mmap
    struct
    {
        uintptr_t   addr;
        size_t      size;
        uint32_t    type;
    }mmap[32];

    //modules
    struct
    {
        uintptr_t   addr;
        size_t     size;
        char        *cmdline;
    }mods[32];

    struct{
        uint8_t  framebuffer_type;
        uintptr_t framebuffer_addr;
        uint32_t framebuffer_pitch;
        uint32_t framebuffer_width;
        uint32_t framebuffer_height;
        uint32_t framebuffer_bpp;

        struct fb_bitfield red;
        struct fb_bitfield blue;
        struct fb_bitfield green;
        struct fb_bitfield resv;
    }framebuffer;

    multiboot_info_t mbi; // save data
    uintptr_t free_physaddr;
}bootinfo_t;

extern bootinfo_t bootinfo;


typedef struct mmap_struct
{
    u64_t addr;
    u64_t size;
    uint32_t type;
}__attribute__((packed)) mmap_struct_t;

typedef struct mod_struct
{
    uint32_t addr;
    uint32_t size;
    uint32_t string;
}__attribute__((packed)) mod_struct_t;

typedef struct __boot_info
{
    uint32_t flags; 
    u64_t mem_top;
    u64_t mem_size;
    mmap_struct_t memory_map[32];
    uint32_t mmap_cnt;
    mod_struct_t module[32];
    uint32_t mods_cnt;
} __attribute__((packed)) boot_info_t;

extern boot_info_t boot_info;

extern multiboot_info_t *mboot;

#endif //__BOOT__