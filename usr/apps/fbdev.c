#include <ginger.h>


struct fb_fix_screeninfo finfo;
struct fb_var_screeninfo vinfo;

long screenlen = 0;
int bpp = 0;


int main(int argc, char *argv[])
{
    printf("fbdev\n");

    if (chdir("/dev"))
    {
        printf("failed to change directory\n");
        exit(1);
    }

    int fb = open("fb0", O_WRONLY);

    if (fb < 0)
    {
        printf("failed to open fb0\n");
        exit(1);
    }

    if (ioctl(fb, FBIOGET_FSCREENINFO, &finfo))
    {
        printf("failed to get fixed framebuffer info\n");
        exit(1);
    }

    if (ioctl(fb, FBIOGET_VSCREENINFO, &vinfo))
    {
        printf("failed to get variable framebuffer info\n");
        exit(1);
    }


    screenlen = finfo.line_length * vinfo.yres_virtual;
    bpp = vinfo.bits_per_pixel / 8;

    int pixel[20];
    
    for (int i =0; i < 20; ++i)
        pixel[i] = BLEND_RGBA(RGB_azure, RGB_dim_gray, 0xff);
    

    printf("about to write to fbdev\n");

    write(fb, (char *)pixel, sizeof pixel);

    printf("fbdev done writing to framebuffer\n");
 
    exit(0);
    return 0;
}