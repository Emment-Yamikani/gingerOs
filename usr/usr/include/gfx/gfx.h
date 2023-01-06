#ifndef GFX_GFX_H
#define GFX_GFX_H 1
#include <video/color_code.h>
#include <stdint.h>

#define PUT_BITFIELD(p, b) ((p) << b)

#define PUT_A(p) PUT_BITFIELD(p, 24)
#define PUT_R(p) PUT_BITFIELD(p, 16)
#define PUT_G(p) PUT_BITFIELD(p, 8)
#define PUT_B(p) PUT_BITFIELD(p, 0)

#define COMPOSE_RGB_PIXEL(r, g, b, a) \
    (PUT_A(a) | PUT_R(r) | PUT_G(g) | PUT_B(b))

#define PICK_BITFIELD(p, b) ((p >> b) & 0xff)

#define PICK_A(p) PICK_BITFIELD(p, 24)
#define PICK_R(p) PICK_BITFIELD(p, 16)
#define PICK_G(p) PICK_BITFIELD(p, 8)
#define PICK_B(p) PICK_BITFIELD(p, 0)

#define USE_PIXEL(p, a) \
    COMPOSE_RGB_PIXEL(PICK_R(p), PICK_G(p), PICK_B(p), PICK_A(p))

#define BLEND_BITFIELD(f, b, a) \
    (uint8_t)(((double)f * ((double)a / 255.0)) + ((1.0 - ((double)a / 255.0)) * (double)b))

#define BLEND_RGBA(fg, bg, a) COMPOSE_RGB_PIXEL(BLEND_BITFIELD(PICK_R(fg), PICK_R(bg), a), \
                                                BLEND_BITFIELD(PICK_G(fg), PICK_G(bg), a), \
                                                BLEND_BITFIELD(PICK_B(fg), PICK_B(bg), a), \
                                                BLEND_BITFIELD(PICK_A(fg), PICK_A(bg), a))

#define PICK_PIXEL(l) (*l)

/* graphx rendering thread for the kernel */
void *gfx_thread(void *);

void putpixel(uint8_t *buff, double x, double y, int pixel);
void drawline(uint8_t *buff, int x1, int y1, int x2, int y2, int color);
void drawLine(uint8_t *buff, double x0, double y0, double x1, double y1, int color);
void drawcursor(uint8_t *buff, uint8_t *data, double xpos, double ypos, int xsz, int ysz, int color);

typedef struct
{
    int opacity;
    int cols, rows;
    uint8_t *backbuff;
    uint8_t *textbuff;
} ctx_t;

typedef struct _gfx_ctx_
{
    int bpp; // bytes per pixel
    uint32_t *buffer;
    int width, height;
} gfx_ctx_t;

typedef struct _window_
{
    int x, y;
    int width, height;
    void *node;
    int bg_color;
    int id;
    gfx_ctx_t *ctx;
} window_t;

typedef struct _desktop_
{
    gfx_ctx_t *ctx; // context canvas
    int mouse_x, mouse_y;
    int last_button_state;
    int nwindows;
    window_t *dragged_child;
    int drag_off_x, drag_off_y;
} desktop_t;

desktop_t *desktop_new(gfx_ctx_t *ctx);
void desktop_paint(desktop_t *desktop);

void desktop_mouse_event(desktop_t *desktop, int x, int y, int button);
void desktop_create_window(desktop_t *desktop, int x, int y, int width, int height);
void desktop_process_mouse(desktop_t *desktop, int mouse_x, int mouse_y, int button);

void window_putpixel(window_t *window, int x, int y, int color);
void window_drawline(window_t *window, double x0, double y0, double x1, double y1, int color);
window_t *window_new(int x, int y, int width, int height, gfx_ctx_t *ctx);
void window_paint(window_t *);

void ctx_fillrect(gfx_ctx_t *, int x, int y, int wifth, int height, int color);

#endif // GFX_GFX_H