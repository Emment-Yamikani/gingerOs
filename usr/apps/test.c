#include <ginger.h>

#ifndef MAP_DONTEXPAND
#define MAP_DONTEXPAND 0x0004
#endif

void *framebuffer = NULL;
int **scanlines = NULL;

#define PI 3.14159265358979323846 /* pi */
#define peek_pixel(x, y)    (scanlines[(int)(y)][(int)(x)])
#define set_pixel(x, y, c)  {if ((((x) >= 0) && ((x) < 800)) && (((y) >= 0) && ((y) < 600))) peek_pixel((x), (y)) = (c);}

void fill_circle(float x0, float y0, float r, int color);
void draw_circle(float x0, float y0, float r, int color);
void draw_line(float x, float y, float x1, float y1, int color);

int main(int argc, char *const argv[])
{
    dup2(open("/dev/uart", O_WRONLY), 2);
    int fb = open("/dev/fbdev", O_RDWR);
    size_t fbsz = lseek(fb, 0, SEEK_END);
    framebuffer = mmap(NULL, fbsz, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_DONTEXPAND, fb, 0);

    scanlines = calloc(600, sizeof (int *));
    int *backbuffer = calloc(1, 800 * 600 * 4);
    for (int sl = 0; sl < 600; ++sl)
        scanlines[sl] = &((int *)framebuffer)[sl * 800];

    draw_circle(400, 300, 100, RGB_cyan);
    //memcpy(framebuffer, backbuffer, fbsz);
    munmap(framebuffer, fbsz);
    return 0;
}

void draw_circle(float x0, float y0, float r, int color)
{
    float x, y;
    //y  = /r2 - x2

    for (float alpha = -PI; alpha < PI; alpha += .01)
    {
        y = 600 - r * sin(alpha) - y0;
        x = r * cos(alpha) + x0;
        set_pixel(x, y, color);
    }
}

void draw_line(float x, float y, float x1, float y1, int color)
{
    float t = 0, c = 0, m = 0;
    
    if (x > x1)
    {
        t = x; x = x1, x1 = t;
        t = y; y = y1, y1 = t;
    }

    if ((x1 - x) == 0) goto infinite_grad;

    m = ((y1 - y) / (x1 - x));
    c = y - (m*x);

    // y1 - y = m(x1 - x)
    // y1 = m*x1 - m*x + y

    for (; x < x1; x += .1)
    {
        y = 600.0 -(m * x + c);
        //dprintf(2, "(%0.3f, %0.3f), c = %0.6f, m = %0.3f\n", x, y, c, m);
        set_pixel(x, y, color);
    }

    return;
infinite_grad:
    if (y > y1) {t = y; y = y1, y1 = t;}
    for (; y < y1; y += .1)
        set_pixel(x, y, color);
}

void fill_circle(float x0, float y0, float r, int color)
{
    float x, y;
    // y  = /r2 - x2

    /*for (float alpha = -PI; alpha < PI; alpha += .00001)
    {
        y = 600 - r * sin(alpha) - y0;
        x = r * cos(alpha) + x0;
        draw_line(x0, y0, x, y, color);
        
    }*/

    for (; r > 0.0 ; r -= .1){
        draw_circle(x0, y0, r, color);
    }
}