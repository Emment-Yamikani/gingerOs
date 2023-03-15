#include <ginger.h>

#ifndef MAP_ZERO
#define MAP_ZERO  0x0010
#endif
#ifndef MAP_MAPIN
#define MAP_MAPIN 0x0020
#endif


int **backbuffer = NULL;

#define _point(fbd, x, y) (fbd[(int)y][(int)x])
#define set_pixel(x, y, c) (_point(backbuffer, x, y) = c)
#define peek_pixel(x, y) (_point(backbuffer, x, y))

void draw_box(int x, int y, int w, int h, int color, int op);
void draw_quadratic(int x0, int x, int c, int color, int op);
void draw_circle(double x, double y, double r, int color, int op);
void draw_line(double x0, double y0, double x, double y, int color, int op);
void draw_x2(double x0, double x, double stretch, double rx, double ry, int color, int op);
void draw_x3(double x0, double x, double stretch, double rx, double ry, int color, int op);

long read_timer(void);

int rtc0 = 0;

int main(int argc, const char *argv[])
{
    dup2(open("/dev/uart", O_WRONLY), 2);
    int fb = open("/dev/fbdev", O_RDWR);
    rtc0 = open("/dev/rtc0", O_RDONLY);
    size_t fbsz = lseek(fb, 0, SEEK_END);

    int *bmap = NULL, *framebuffer = mmap((void *)0xA000000, fbsz,
                     PROT_WRITE | PROT_READ, MAP_SHARED, fb, 0);

    bmap = mmap(NULL, fbsz, PROT_READ | PROT_WRITE, MAP_ANON | MAP_MAPIN | MAP_PRIVATE, -1, 0);
    backbuffer = calloc(600, sizeof(int **));
    for (int i = 0; i < 600; ++i)
        backbuffer[i] = &bmap[i * 800];

    draw_line(0, 300, 800, 300, RGB_white, 200);
    draw_line(400, 0, 400, 600, RGB_white, 200);

    long time = read_timer();
    draw_x2(-200.0, 800, .01, 400, 300, RGB_dark_red, 200);
    dprintf(2, "time: %dsec\n", read_timer() - time);

    memcpy(framebuffer, bmap, fbsz);

    return 0;
}

void draw_box(int x, int y, int w, int h, int color, int op)
{
    int xmx = x + w;
    int ymx = y + h;

    x = x < 0 ? 0 : x > 800 ? 800
                            : x;
    y = y < 0 ? 0 : y > 600 ? 600
                            : y;

    xmx = xmx < 0 ? 0 : xmx > 800 ? 800
                                  : xmx;
    ymx = ymx < 0 ? 0 : ymx > 600 ? 600
                                  : ymx;

    for (; y < ymx; ++y)
    {
        for (int i = x; i < xmx; ++i)
        {
            set_pixel(i, y, BLEND_RGBA(color, peek_pixel(i, y), op));
        }
    }
}

void draw_line(double x0, double y0, double x, double y, int color, int op)
{
    double m = 0, c = 0;
    double xt = x, yt = y;

    if (x0 > x)
    {
        xt = x0, x0 = x, x = xt;
        yt = y0, y0 = y, y = yt;
    }

    if ((x - x0) == 0)
        goto init_grad;

    m = -((y - y0) / (x - x0));
    c = y0 - (m * x0);

    for (x = x0; x < xt; x += .1)
    {
        y = (m * x) + c + (m > 0 ? -600 : m < 0 ? 600
                                                : 0);
        // dprintf(2, "(%0.2f, %0.2f), (%0.2f, %0.2f). m= %0.2f, c= %0.2f\n", x, y, xt, yt, m, c);
        draw_box(x, y, 1, 1, color, op);
    }
    return;
init_grad:
    if (y0 > y)
    {
        yt = y0, y = y0, y = yt;
    }

    for (y = y0; y < yt; y += .1)
        draw_box(x, 600 - y, 1, 1, color, op);
}

void draw_x2(double x0, double x, double stretch, double rx, double ry, int color, int op)
{
    double y = 0;
    for (; x0 < x; x0 += .01)
    {
        y = 600.0 - (stretch * (x0) * (x0)) - ry;
        draw_box(x0 + rx, y, 10, 10, color, op);
    }
}

void draw_x3(double x0, double x, double stretch, double rx, double ry, int color, int op)
{
    double y = 0;
    for (; x0 < x; x0 += .01)
    {
        y = 600.00 - (stretch * (x0) * (x0) * (x0)) - ry;
        draw_box(x0 + rx, y, 1, 1, color, op); 
    }
}

void draw_circle(double x, double y, double r, int color, int op)
{
    double cx = x, cy = y, xm = 0;
    x = cx - r;
    xm = cx + r;

    for (; x < xm; x += 0.1)
    {
        //y = r * sin(0.5*x);
        draw_box(x, y, 1, 1, color, op);
    }
}

long read_timer(void)
{
    long timer = 0;
    read(rtc0, &timer, sizeof timer);
    return timer;
}