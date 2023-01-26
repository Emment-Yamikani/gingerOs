#ifndef __BOOT__
#define __BOOT__
#include "multiboot.h"
#include <lib/stdint.h>
#include <lib/stddef.h>
#include <lib/types.h>
#include <dev/fb.h>

typedef struct bootinfo
{
    int     mmap_count;
    int     mods_count;
    size_t  pmemsize;   //memory size in Kb
    char    *kcmdline;
    //mmap
    struct
    {
        uintptr_t   addr;
        size_t     size;
        int         type;
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
}bootinfo_t;

extern bootinfo_t bootinfo;

#endif //__BOOT__