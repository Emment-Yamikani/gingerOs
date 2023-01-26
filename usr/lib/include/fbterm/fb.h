#ifndef _FBTERM_FB_H
#define _FBTERM_FB_H

#include <stdint.h>
#include <fbterm/fbterm.h>

#define FBIOGET_FIX_INFO 0x0000
#define FBIOGET_VAR_INFO 0x0001

struct fb_bitfield
{
    uint32_t offset;    // position in pixel
    uint32_t length;    // length of bitfield
    uint32_t msb_right; // true if most significant byte is right
};

typedef struct fb_fixinfo
{
    char id[64];        // indentification
    int accel;          // type of acceleration card in use
    uint32_t type;      // type of framebuffer
    uint32_t caps;      // capabilities
    size_t memsz;       // total memory size of framebuffer
    uintptr_t addr;     // physical address of framebuffer
    size_t line_length; // bytes per line
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

void fb_put_pixel(struct fbterm_ctx *ctx, int x, int y, uint32_t fg, uint32_t bg);
void fb_clear(struct fbterm_ctx *ctx);
void fb_render(struct fbterm_ctx *ctx);
void fb_term_init(struct fbterm_ctx *ctx);
int fb_init(char *path);
int fb_cook_wallpaper(struct fbterm_ctx *ctx, char *path);
void fb_rect_clear(struct fbterm_ctx *ctx, size_t x0, size_t x1, size_t y0, size_t y1);
void fb_rect_move(struct fbterm_ctx *ctx, size_t dx0, size_t dx1, size_t dy0, size_t dy1,
        size_t sx0, size_t sx1, size_t sy0, size_t sy1);

#define _RGBA(r, g, b, a) (((r) << 3*8) | ((g) << 2*8) | ((b) << 1*8) | ((a) << 0*8))
#define _RGB(r, g, b) (((r) << 3*8) | ((g) << 2*8) | ((b) << 1*8))
#define _ALPHA(c, a) (((c) & (~0xFF)) | ((a) & 0xFF))

#endif
