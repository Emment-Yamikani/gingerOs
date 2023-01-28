#ifndef DEV_PS2MOUSE_H
#define DEV_PS2MOUSE_H 1
#include <sys/system.h>
#include <lib/stdint.h>

TYPE(_mouse_packet_)
{
    union 
    {
        struct
        {
            uint8_t lb :1;
            uint8_t rb :1;
            uint8_t mb :1;
            uint8_t A1 :1;
            uint8_t xs :1;
            uint8_t ys :1;
            uint8_t xo :1;
            uint8_t yo :1;
        };
        uint8_t raw;
    };
    uint8_t rel_x;
    uint8_t rel_y;
    uint8_t zaxis;
} mouse_packet_t;

TYPE(_mouse_data_)
{
    int buttons;
    int x, y, z;
} mouse_data_t;

typedef enum
{
    LEFT_CLICK = 0x01,
    RIGHT_CLICK = 0x02,
    MIDDLE_CLICK = 0x04
} mouse_click_t;

typedef struct
{
    uint32_t magic;
    int8_t x_difference;
    int8_t y_difference;
    mouse_click_t buttons;
} mouse_device_packet_t;

#define MOUSE_MAGIC 0xFEED1234

int ps2mouse_init(void);

#endif // DEV_PS2MOUSE_H