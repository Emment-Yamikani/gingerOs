
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


#endif // GFX_GFX_H