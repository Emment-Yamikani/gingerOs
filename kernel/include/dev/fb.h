#ifndef _DEV_FB_H
#define _DEV_FB_H 1

#include <lib/stdint.h>
#include <lib/stddef.h>
#include <locks/spinlock.h>
#include <dev/dev.h>


#define NFBDEV  8

#define FBIOGET_FIX_INFO  0x0000
#define FBIOGET_VAR_INFO  0x0001

struct fb_bitfield
{
    uint8_t offset;    // position in pixel
    uint8_t length;    // length of bitfield
    uint8_t msb_right; // true if most significant byte is right
};

typedef struct fb_fixinfo
{
    char        id[64];     // indentification
    int         accel;      // type of acceleration card in use
    uint32_t    type;       // type of framebuffer
    uint32_t    caps;       // capabilities
    size_t      memsz;      // total memory size of framebuffer
    uintptr_t   addr;       // physical address of framebuffer
    size_t      line_length;// bytes per line
} fb_fixinfo_t;

typedef struct fb_varinfo
{
    size_t bpp;          // bits per pixel
    size_t width;        // pixels per row
    size_t height;       // pixels per column
    uint32_t vmode;      // video mode
    uint32_t pitch;      // pitch
    uint32_t grayscale;  // greyscaling
    uint32_t colorspace; // colorspace (e.g, RBGA, e.t.c)

    /* bitfield if true color */

    struct fb_bitfield red;
    struct fb_bitfield blue;
    struct fb_bitfield green;
    struct fb_bitfield transp;
} fb_varinfo_t;

typedef struct framebuffer
{
    uint32_t  id;
    void *priv;
    struct dev *  dev;
    fb_fixinfo_t *fixinfo;
    fb_varinfo_t *varinfo;
    spinlock_t *  lock;
    void *module;
} framebuffer_t;

int framebuffer_process_info();

#endif //_DEV_FB_H