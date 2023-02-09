#include <ginger.h>
#include <glist.h>

#include "nanojpeg.c"

typedef struct _mouse_data_
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
    int x, y, z;
} point_t;

typedef struct
{
    int width;
    int height;
} dimensions_t;

typedef struct
{
    uint32_t *buffer;
    int bpp;
    int width;
    int height;
    size_t memsize;
} context_t, *Context;

struct __desktop__;

typedef struct
{
    int default_color;
    uint8_t opacity;
    int traparent;
    point_t pos;       // position of window on desktop
    dimensions_t size; // window dimensions(col pixels * row pixels)
    context_t context; // drawing buffer
    struct __desktop__ *desktop;
} window_t, *Window;

typedef struct __desktop__
{
    int window_count;
    glist_t list;
    Window mouse;
    Window background;
    int last_buttons;
    uint32_t *framebuffer;
    Window drag_window;
    point_t drag_offset;
} desktop_t, *Desktop;

int cook_image(char *path, int xres, int yres, int depth, uint8_t *img_data);
int context_fillrect(Context, int x, int y, int width, int height, int color);

int desktop_new(Desktop *ref, int width, int height, int bpp);
int desktop_redraw(Desktop);
int desktop_render(Desktop);
int desktop_new_window(Desktop, int x, int y, int width, int height, int color, Window *);

int window_init(Window win);
int window_draw(Window win);
int window_fill(Window win, int color);
int window_reposition(Window, int x, int y);
int window_resize(Window, int width, int height);
int window_new(Window *, int x, int y, int width, int height, int default_color);

void *thread_mouse(void *arg);

int fb = 0;
int mouse = 0;
Desktop desktop;
fb_varinfo_t varinfo;
fb_fixinfo_t fixinfo;

Window taskbar = NULL;
uint32_t *framebuffer = NULL;
int default_color = RGB_light_gray;

int main(int argc, const char *argv[])
{
    close(0);
    close(1);
    close(2);
    close(3);

    char cwd[1024] = {0};
    getcwd(cwd, sizeof cwd);
    chdir("/dev");

    open("kbd0", O_RDONLY);
    open("uart", O_WRONLY);
    open("uart", O_WRONLY);
    fb = open("fbdev", O_RDWR);
    mouse = open("event0", O_RDONLY);
    chdir(cwd);

    ioctl(fb, FBIOGET_FIX_INFO, &fixinfo);
    ioctl(fb, FBIOGET_VAR_INFO, &varinfo);

    printf("Window manager.\n");

    desktop_new(&desktop, varinfo.width, varinfo.height, varinfo.bpp);

    Window window  = desktop->background;
    cook_image("art.jpg",
        window->size.width,
        window->size.height,
        window->context.bpp / 8,
        (uint8_t *)window->context.buffer);

    desktop_new_window(desktop, 200, 100, 460, 400, RGB_sea_green, &window);
    cook_image("wall.jpg",
               window->size.width,
               window->size.height,
               window->context.bpp / 8,
               (uint8_t *)window->context.buffer);
    window->opacity = 200;

    tid_t tid = 0;
    thread_create(&tid, thread_mouse, &tid);
    
    while (1)
    {
        desktop_render(desktop);
    }
    
    thread_join(tid, (void *)&tid);
    return 0;
}

int cook_image(char *path, int xres, int yres, int depth, uint8_t *img_data)
{
    int img = open(path, O_RDONLY);
    size_t size = lseek(img, 0, SEEK_END);
    lseek(img, 0, SEEK_SET);

    char *buf = malloc(size);
    read(img, buf, size);
    close(img);

    int err = 0;

    njInit();
    if (err = njDecode(buf, size))
    {
        free(buf);
        // fprintf(stderr, "Error decoding input file: %d\n", err);
        printf("Error decoding input file: %d\n", err);
        return -1;
    }

    free(buf);
    size_t pitch = xres * depth;
    size_t height = njGetHeight();
    size_t width = njGetWidth();
    size_t cook_height, cook_width;
    char *img_buf = njGetImage();
    size_t ncomp = njGetImageSize() / (height * width);
    size_t img_line_length = width * ncomp;
    size_t xpan = 0, ypan = 0, xoffset = 0, yoffset = 0;

    /* Pan or crop image to match screen size */
    if (width < xres)
    {
        xpan = (xres - width) / 2;
        cook_width = width;
    }
    else
    {
        xoffset = (width - xres) / 2;
        cook_width = xres;
    }

    if (height < yres)
    {
        ypan = (yres - height) / 2;
        cook_height = height;
    }
    else
    {
        yoffset = (height - yres) / 2;
        cook_height = yres;
    }

#define WP_POS(i, j) (((i) + ypan) * pitch + ((j) + xpan) * depth)
#define IMG_POS(i, j) (((i) + yoffset) * img_line_length + ((j) + xoffset) * ncomp)

    for (size_t i = 0; i < cook_height; ++i)
    {
        for (size_t j = 0; j < cook_width; ++j)
        {
            img_data[WP_POS(i, j) + 2] = img_buf[IMG_POS(i, j) + 0];
            img_data[WP_POS(i, j) + 1] = img_buf[IMG_POS(i, j) + 1];
            img_data[WP_POS(i, j) + 0] = img_buf[IMG_POS(i, j) + 2];
        }
    }

    njDone();

    return 0;
}

int blit(Desktop top, Context ctx, int x, int y, int transp, uint8_t opacity)
{
    if (ctx == NULL)
        return -EINVAL;
    for (int yy = 0; yy < ctx->height; ++yy)
    {
        for (int xx = 0; xx < ctx->width; ++xx)
        {
            if (ctx->bpp == 32)
            {
                if (transp)
                {
                    int bg = PICK_PIXEL(&top->framebuffer[(xx + x) + (y + yy) * top->background->size.width]);
                    int fg = ctx->buffer[xx + yy * ctx->width];
                    top->framebuffer[(xx + x) + (y + yy) * top->background->size.width] = BLEND_RGBA(fg, bg, opacity);
                }
                else
                    top->framebuffer[(xx + x) + (y + yy) * top->background->size.width] = ctx->buffer[xx + yy * ctx->width];
            }
        }
    }

    return 0;
}

int context_fillrect(Context ctx, int x, int y, int width, int height, int color)
{
    int cx = 0, xx = 0, yy = 0;
    if (ctx == NULL)
        return -EINVAL;

    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
        

    xx = x + width;
    yy = y + height;

    if (xx >= ctx->width)
        xx = ctx->width;

    if (yy >= ctx->height)
        yy = ctx->height;

    for (; y < yy; ++y)
        for (cx = x; cx < xx; ++cx)
            ctx->buffer[cx + y * ctx->width] = color;
    return 0;
}

int desktop_new(Desktop *ref, int width, int height, int bpp)
{
    int err = 0;
    Desktop top = NULL;
    void *buffer = NULL;

    if (ref == NULL)
        return -EINVAL;

    if ((top = malloc(sizeof *top)) == NULL)
        return -ENOMEM;

    if ((buffer = calloc(width * height, bpp / 8)) == NULL)
    {
        free(top);
        return -ENOMEM;
    }

    memset(top, 0, sizeof *top);

    if ((err = window_new(&top->background, 0, 0, width, height, RGB_black)))
    {
        free(buffer);
        free(top);
        return err;
    }

    top->framebuffer = buffer;
    top->background->desktop = top;
    top->background->traparent = 0;

    if ((err = window_new(&top->mouse, width / 2, height / 2, 10, 10, RGB_white)))
    {
        free(top->background->context.buffer);
        free(top->background);
        free(buffer);
        free(top);
        return err;
    }

    top->mouse->desktop = top;
    top->mouse->traparent = 0;

    *ref = top;
    return 0;
}

int desktop_redraw(Desktop top)
{
    Window win;
    window_draw(top->background);
    forlinked(node, top->list.head, node->next)
    {
        win = node->ptr;
        window_draw(win);
    }
    window_draw(top->mouse);

    return 0;
}

int desktop_render(Desktop top)
{
    desktop_redraw(top);
    lseek(fb, 0, SEEK_SET);
    write(fb, top->framebuffer, top->background->context.memsize);
    lseek(fb, 0, SEEK_SET);
}

int desktop_new_window(Desktop top, int x, int y, int width, int height, int color, Window *ref)
{
    int err = 0;
    Window win = NULL;
    if (top == NULL)
        return -EINVAL;
    if ((err = window_new(&win, x, y, width, height, color)))
        return err;

    win->desktop = top;

    if ((err = glist_add(&top->list, win)))
    {
        free(win->context.buffer);
        free(win);
        return err;
    }

    top->window_count++;
    if (ref)
        *ref = win;
    return 0;
}

int window_fill(Window win, int color)
{
    if (win == NULL)
        return -EINVAL;
    return context_fillrect(&win->context, 0, 0, win->size.width, win->size.height, color);
}

int window_draw(Window win)
{
    if (win == NULL)
        return -EINVAL;
    return blit(win->desktop, &win->context, win->pos.x, win->pos.y, win->traparent, win->opacity);
}

int window_new(Window *ref, int x, int y, int width, int height, int default_color)
{
    point_t pos;
    dimensions_t size;
    Window win = NULL;
    void *buffer = NULL;

    if (ref == NULL)
        return -EINVAL;

    if ((win = malloc(sizeof *win)) == NULL)
        return -ENOMEM;

    memset(win, 0, sizeof *win);

    if ((buffer = calloc((height * width), (varinfo.bpp / 8))) == NULL)
    {
        free(win);
        return -ENOMEM;
    }

    pos.x = x;
    pos.y = y;
    pos.z = 0;
    size.width = width;
    size.height = height;

    win->default_color = default_color;
    win->context.bpp = varinfo.bpp;
    win->context.width = width;
    win->context.height = height;
    win->context.buffer = buffer;
    win->context.memsize = (width * height) * (varinfo.bpp / 8);
    win->pos = pos;
    win->size = size;
    win->opacity = 60;
    win->traparent = 1;

    *ref = win;
    return window_init(win);
}

int window_reposition(Window win, int x, int y)
{
    if (win == NULL)
        return -EINVAL;
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;

    if (x >= win->desktop->background->size.width)
        x = win->desktop->background->size.width - win->size.width - 1;

    if (y >= win->desktop->background->size.height)
        y = win->desktop->background->size.height - win->size.height - 1;

    win->pos = (point_t){x, y, 0};
    return 0;
}

int window_init(Window win)
{
    if (win == NULL)
        return -EINVAL;
    return window_fill(win, win->default_color);
}

int window_resize(Window win, int width, int height)
{
    void *buffer = NULL;
    if (win == NULL)
        return -EINVAL;

    if ((buffer = malloc(width * height * (win->context.bpp / 8))) == NULL)
        return -ENOMEM;

    free(win->context.buffer);
    win->context.buffer = buffer;
    win->context.height = height;
    win->size = (dimensions_t){width, height};
    return window_fill(win, default_color);
}

void mouse_process(point_t rel, int buttons)
{
    point_t pos;
    Window window = NULL;

    pos = desktop->mouse->pos;
    pos.x += rel.x;
    pos.y -= rel.y;
    window_reposition(desktop->mouse, pos.x, pos.y);

    if (buttons)
    {
        if (!desktop->last_buttons)
        {
            forlinked(node, desktop->list.tail, node->prev)
            {
                window = node->ptr;

                if ((pos.x > window->pos.x) && (pos.x < (window->pos.x + window->size.width)) &&
                    (pos.y > window->pos.y) && (pos.y < (window->pos.y + window->size.height)))
                {
                    glist_del(&desktop->list, window);
                    glist_add(&desktop->list, window);

                    desktop->drag_window = window;
                    pos = (point_t){pos.x - window->pos.x, pos.y - window->pos.y};
                    desktop->drag_offset = pos;
                    break;
                }
            }
        }
    }
    else
        desktop->drag_window = NULL;

    if (desktop->drag_window)
    {
        pos = (point_t){
            desktop->mouse->pos.x - desktop->drag_offset.x,
            desktop->mouse->pos.y - desktop->drag_offset.y,
        };
        window_reposition(desktop->drag_window, pos.x, pos.y);
    }

    desktop->last_buttons = buttons;
}

void *thread_mouse(void *arg)
{
    (void)arg;
    point_t pos;
    mouse_data_t data;
    int buttons = 0;

    while (1)
    {
        read(mouse, &data, sizeof data);
        if (pos.x == data.x && pos.y == data.y && buttons == data.buttons)
            continue;
        buttons = data.buttons;
        pos = (point_t){data.x, data.y, 0};
        mouse_process(pos, data.buttons);
    }
}