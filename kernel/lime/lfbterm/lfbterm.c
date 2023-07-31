#include <dev/fb.h>
#include <fs/fs.h>
#include <sys/kthread.h>
#include <fs/sysfile.h>
#include <mm/kalloc.h>
#include <lib/nanojpeg.c>
#include <font/tinyfont.h>
#include <printk.h>
#include <mm/vmm.h>
#include <mm/liballoc.h>
#include <bits/errno.h>
#include <lime/jiffies.h>
#include <video/color_code.h>
#include <sys/sleep.h>
#include <locks/sync.h>
#include <video/lfbterm.h>

typedef struct lfb_ctx
{
    int         wallbg;
    struct font *fontdata;
    int         textlines;
    int         lfb_textcc;
    int         lfb_textcr;
    uint8_t     *wallpaper;
    int         textpointer;
    uint8_t     cursor_char;
    uint32_t    **scanline0;
    uint32_t    **scanline1;
    char        **textscanline;
    uint8_t     cursor_timeout;
    char        *lfb_textbuffer;
    uint32_t    *lfb_backbuffer;
    uint32_t    *lfb_background;
    uint32_t    *lfb_frontbuffer;
    int         op, fg, bg, transp;
    int         cols, rows, cc, cr;
    spinlock_t  *lock;
} lfb_ctx_t;

lfb_ctx_t ctx;
fb_fixinfo_t fbfix;
fb_varinfo_t fbvar;
inode_t *lfb = NULL;
inode_t *lfb_img = NULL;
volatile int use_gfx_cons = 0;

const char *wallpaper_path[] = {
    "snow_800x600.jpg",
    "snow_1024x768.jpg",
    NULL,
};

#define __peek_pixel(f, x, y) ((f)[(int)(y)][(int)(x)])
#define __set_pixel(f, x, y, c) (__peek_pixel((f), (x), (y)) = (c))
#define __put_pixel(f, x, y, c) ({if (((x) < (int)fbvar.width) &&\
                                     ((y) < (int)fbvar.height) && ((x) >= 0) && ((y) >= 0))\
                                     __set_pixel((f), (x), (y), (c)); })
#define __get_pixel(f, x, y) ({uint32_t px = 0; if (((x) < (int)fbvar.width) &&\
                                     ((y) < (int)fbvar.height) && ((x) >= 0) && ((y) >= 0))\
                                     px = __peek_pixel((f), (x), (y)); px; })

#define __text_peek(buff, x, y) ((buff)[(int)(y)][(int)(x)])
#define __text_setc(buff, x, y, c) (__text_peek((buff), (x), (y)) = (c))
#define __text_putc(buff, x, y, c) ({if (((x) < (int)ctx.cols) && ((y) < (int)ctx.rows)\
                                        && ((x) >= 0) && ((y) >= 0))\
                                            __text_setc((buff), (x), (y), (c)); })

#define __text_getc(buff, x, y) ({if (((x) < (int)ctx.cols) && ((y) < (int)ctx.rows)\
                                        && ((x) >= 0) && ((y) >= 0))\
                                            int c = __text_peek((buff), (x), (y)); c;})

int lfbterm_cook_wallpaper(struct lfb_ctx *ctx, const char *path);

void lfbterm_render(void) {
    for (int y = 0; y < (int)fbvar.height; ++y)
        for (int x = 0; x < (int)fbvar.width; ++x)
            __set_pixel(ctx.scanline1, x, y, __peek_pixel(ctx.scanline0, x, y));
}

int lfbterm_init(void)
{
    uio_t uio = {0};
    int err = -ENOMEM;
    char *wall_path = NULL;
    int cols = 0, rows = 0;

    if ((err = vfs_open("/dev/fbdev", &uio, O_RDWR, 0, &lfb)))
    {
        printk("%s:%d: error, fallback to text_mode\n", __FILE__, __LINE__);
        return err;
    }

    iioctl(lfb, FBIOGET_FIX_INFO, &fbfix);
    iioctl(lfb, FBIOGET_VAR_INFO, &fbvar);

    memset(&ctx, 0, sizeof ctx);

    if ((err = spinlock_init(NULL, "lfbterm", &ctx.lock)))
        goto error;

    err = -EINVAL;

    if (!(ctx.fontdata = font_open("/font.tf")))
        goto error;

    cols = (fbvar.width / ctx.fontdata->cols);
    rows = (fbvar.height / ctx.fontdata->rows);

    err = -ENOMEM;

    if (!(ctx.scanline0 = kcalloc(fbvar.height, (fbvar.bpp / 8))))
        goto error;

    if (!(ctx.scanline1 = kcalloc(fbvar.height, (fbvar.bpp / 8))))
        goto error;

    if (!(ctx.lfb_backbuffer = kcalloc(1, fbfix.memsz)))
        goto error;

    if (!(ctx.lfb_textbuffer = kcalloc(3 * cols *  rows, sizeof(char))))
        goto error;

    if (!(ctx.textscanline = kcalloc(3 * rows, sizeof (char *))))
        goto error;

    for (size_t row = 0; row < fbvar.height; row++)
        ctx.textscanline[row] = &((char *)ctx.lfb_textbuffer)[row * cols];

    err = -EINVAL;

    if ((fbvar.height == 600 || fbvar.width == 800))
        wall_path = (char *)wallpaper_path[0];
    else if ((fbvar.height == 768) || (fbvar.width == 1024))
        wall_path = (char *)wallpaper_path[1];
    else goto error;

    if ((err = lfbterm_cook_wallpaper(&ctx, wall_path)))
        goto error;

    ctx.lfb_frontbuffer = (void *)fbfix.addr;
    ctx.lfb_background = (void *)ctx.wallpaper;

    for (size_t row = 0; row < fbvar.height; row++)
        ctx.scanline0[row] = &((uintptr_t *)ctx.lfb_background)[row * fbvar.width];

    for (size_t row = 0; row < fbvar.height; row++)
        ctx.scanline1[row] = &((uintptr_t *)ctx.lfb_frontbuffer)[row * fbvar.width];

    ctx.op = 200;
    ctx.cols = cols;
    ctx.rows = rows;
    ctx.bg = 0x002B36; // RGB_black;
    ctx.bg = RGB_black;
    ctx.textpointer = 0;
    ctx.cursor_char = '|';
    ctx.cursor_timeout = 15;
    ctx.fg = RGB_alice_blue;

    use_gfx_cons = 1;

    lfbterm_clrscrn();
    return 0;
error:
    if (ctx.lfb_textbuffer)
        kfree(ctx.lfb_textbuffer);
    if (ctx.lfb_backbuffer)
        kfree(ctx.lfb_backbuffer);
    if (ctx.scanline0)
        kfree(ctx.scanline0);
    if (ctx.textscanline)
        kfree(ctx.textscanline);
    return err;
}

int lfbterm_cook_wallpaper(struct lfb_ctx *ctx, const char *path)
{
    int err = 0;

    if ((err = vfs_open(path, NULL, O_RDONLY, 0, &lfb_img)))
        return err;

    size_t size = lfb_img->i_size;

    char *buf = kcalloc(1, size);
    iread(lfb_img, 0, buf, size);
    iclose(lfb_img);


    njInit();

    if ((err = njDecode(buf, size)))
    {
        kfree(buf);
        // fprintf(stderr, "Error decoding input file: %d\n", err);
        printk("Error decoding input file: %d\n", err);
        return err;
    }

    kfree(buf);
    size_t height = njGetHeight();
    size_t width = njGetWidth();
    size_t cook_height, cook_width;
    uint8_t *img_buf = njGetImage();
    size_t ncomp = njGetImageSize() / (height * width);
    size_t img_line_length = width * ncomp;
    size_t xpan = 0, ypan = 0, xoffset = 0, yoffset = 0;

    /* Pan or crop image to match screen size */
    if (width < fbvar.width)
    {
        xpan = (fbvar.width - width) / 2;
        cook_width = width;
    }
    else
    {
        xoffset = (width - fbvar.width) / 2;
        cook_width = fbvar.width;
    }

    if (height < fbvar.height)
    {
        ypan = (fbvar.height - height) / 2;
        cook_height = height;
    }
    else
    {
        yoffset = (height - fbvar.height) / 2;
        cook_height = fbvar.height;
    }

#define WP_POS(i, j) (((i) + ypan) * fbfix.line_length + ((j) + xpan) * (fbvar.bpp / 8))
#define IMG_POS(i, j) (((i) + yoffset) * img_line_length + ((j) + xoffset) * ncomp)

    ctx->wallpaper = kcalloc(1, fbvar.height * fbfix.line_length);

    size = fbvar.height * fbfix.line_length;

    for (size_t i = 0; i < cook_height; ++i)
    {
        for (size_t j = 0; j < cook_width; ++j)
        {
            ctx->wallpaper[WP_POS(i, j) + 2] = img_buf[IMG_POS(i, j) + 0];
            ctx->wallpaper[WP_POS(i, j) + 1] = img_buf[IMG_POS(i, j) + 1];
            ctx->wallpaper[WP_POS(i, j) + 0] = img_buf[IMG_POS(i, j) + 2];
        }
    }

    njDone();

    return 0;
}

int font_putc(int c, struct lfb_ctx *ctx, int col, int row)
{
    char glyph[ctx->fontdata->rows * ctx->fontdata->cols];
    font_bitmap(ctx->fontdata, glyph, c);
    for (int i = 0; i < ctx->fontdata->rows; ++i)
    {
        int cx = col * ctx->fontdata->cols;
        for (int j = 0; j < ctx->fontdata->cols; ++j)
        {
            char v = glyph[i * ctx->fontdata->cols + j];
            for (int b = 0; b < 8; ++b) {
                __put_pixel(ctx->scanline1, cx, (row * ctx->fontdata->rows + i), ((v & (1 << b)) ? ctx->fg
                    : (ctx->wallbg ? (int)__get_pixel(ctx->scanline0, cx, (row * ctx->fontdata->rows + i + 1)) : ctx->bg)));
            }
            ++cx;
        }
    }
    return 0;
}

void ctx_putchar(struct lfb_ctx *ctx, int c);
int cxx = 0;

void ctx_drawcursor(struct lfb_ctx *ctx)
{
    loop()
    {
        spin_lock(ctx->lock);
        font_putc(ctx->cursor_char, ctx, ctx->cc, ctx->cr);
        spin_unlock(ctx->lock);
        timed_wait(ctx->cursor_timeout);
        spin_lock(ctx->lock);
        font_putc(' ', ctx, ctx->cc, ctx->cr);
        spin_unlock(ctx->lock);
        timed_wait(ctx->cursor_timeout);
    }
}

void lfbterm_scroll(void);

void ctx_putchar(struct lfb_ctx *ctx, int c)
{
    font_putc(' ', ctx, ctx->cc, ctx->cr);
    switch (c)
    {
    case '\n':
        ctx->cc = 0;
        ctx->cr++;
        break;
    case '\t':
        ctx->cc = (ctx->cc + 4) & ~3;
        if (ctx->cc >= ctx->cols)
        {
            ctx->cr++;
            ctx->cc = 0;
        }
        break;
    case '\r':
        ctx->cc = 0;
        break;
    case '\b':
        ctx->cc--;
        if (ctx->cc < 0)
        {
            ctx->cr--;
            if (ctx->cr >= 0)
                ctx->cc = ctx->cols - 1;
            else
                ctx->cr = ctx->cc = 0;
        }
        font_putc(' ', ctx, ctx->cc, ctx->cr);
        break;
    default:
        font_putc(c, ctx, ctx->cc, ctx->cr);
        ctx->cc++;
        if (ctx->cc >= ctx->cols)
        {
            ctx->cr++;
            ctx->cc = 0;
        }
    }
    if (ctx->cr >= ctx->rows)
        lfbterm_scroll();
}

void ctx_puts(struct lfb_ctx *ctx, char *str)
{
    while (*str)
        ctx_putchar(ctx, *str++);
}

void lfbtext_putc(int c) {
    //__text_putc()
    //char **buff = &ctx.scanline2[ctx.textpointer];
    //__text_putc(buff, ctx.lfb_textcc, ctx.lfb_textcr, c);

    switch (c)
    {
    case '\n':
        ctx.lfb_textcc = 0;
        ctx.lfb_textcr++;
        break;
    case '\t':
        ctx.lfb_textcc = (ctx.lfb_textcc + 4) & ~3;
        if (ctx.lfb_textcc >= ctx.cols)
        {
            ctx.lfb_textcr++;
            ctx.lfb_textcc = 0;
        }
        break;
    case '\r':
        ctx.lfb_textcc = 0;
        break;
    case '\b':
        ctx.lfb_textcc--;
        if (ctx.lfb_textcc < 0)
        {
            ctx.lfb_textcr--;
            if (ctx.lfb_textcr >= 0)
                ctx.lfb_textcc = ctx.cols - 1;
            else
                ctx.lfb_textcr = ctx.lfb_textcc = 0;
        }
        font_putc(' ', &ctx, ctx.lfb_textcc, ctx.lfb_textcr);
        break;
    default:
        font_putc(c, &ctx, ctx.lfb_textcc, ctx.lfb_textcr);
        ctx.lfb_textcc++;
        if (ctx.lfb_textcc >= ctx.cols)
        {
            ctx.lfb_textcr++;
            ctx.lfb_textcc = 0;
        }
    }
    if (ctx.lfb_textcr >= ctx.rows)
        lfbterm_scroll();
}

void lfbterm_putc(int c)
{
    cxx = c;
    switch (c)
    {
    case _SC_UP:
        lfbterm_puts("^[[A");
        return;
    case _SC_LEFT:
        lfbterm_puts("^[[D");
        return;
    case _SC_RIGHT:
        lfbterm_puts("^[[C");
        return;
    case _SC_DOWN:
        lfbterm_puts("^[[B");
        return;
    case CTRL('C'):
        lfbterm_puts("^C");
        return;
    default:
        spin_lock(ctx.lock);

        ctx_putchar(&ctx, c);
        spin_unlock(ctx.lock);
    }
}

void lfbterm_fill_rect(int x, int y, int w, int h, int color)
{
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if ((w < 0) || (h < 0))
        return;

    int xmax = w + x, ymax = h + y;
    if (xmax > (int)fbvar.width)
        xmax = fbvar.width;
    if (ymax > (int)fbvar.height)
        ymax = fbvar.height;

    for (; y < ymax; ++y)
        for (int cx = x; cx < xmax; ++cx)
            __put_pixel(ctx.scanline1, cx, y, color);
}

void lfbterm_clrscrn(void)
{
    spin_lock(ctx.lock);
    if (!ctx.wallpaper)
        lfbterm_fill_rect(0, 0, fbvar.width, fbvar.height, ctx.bg);
    else {
        memcpy(ctx.lfb_frontbuffer, ctx.lfb_background, fbvar.height * fbfix.line_length);
        ctx.wallbg = 1;
    }
    ctx.cc = ctx.cr = 0;
    //lfbterm_render();
    spin_unlock(ctx.lock);
}

int lfbterm_puts(const char *s)
{
    char *S = (char *)s;
    while (*S)
        ctx_putchar(&ctx, *S++);
    return S - s;
}

void lfbterm_scroll(void)
{
    font_putc(' ', &ctx, ctx.cc, ctx.cr);
    for (int row = ctx.fontdata->rows; row < (int)fbvar.height; ++row)
        for (int col = 0; col < (int)fbvar.width; ++col)
            __put_pixel((ctx.scanline1), col, (row - ctx.fontdata->rows), __peek_pixel(ctx.scanline0, col, row));
    for (int row = (int)fbvar.height - ctx.fontdata->rows; row < (int)fbvar.height; ++row)
        for (int col = 0; col < (int)fbvar.width; ++col)
            __put_pixel((ctx.scanline1), col, row, (ctx.wallbg ? (int)__get_pixel(ctx.scanline0, col, row) : ctx.bg));
    ctx.cc = 0;
    ctx.cr = ctx.rows - 1;
}

void lfbterm_setcolor(int bg, int fg)
{
    spin_lock(ctx.lock);
    ctx.bg = bg;
    ctx.fg = fg;
    spin_unlock(ctx.lock);
}

static struct cons_attr
{
    int bg, fg;
    struct cons_attr *prev, *next;
} *tail;

void lfbterm_savecolor(void)
{
    struct cons_attr *node;
    if (!(node = kmalloc(sizeof *node)))
        return;
    memset(node, 0, sizeof *node);
    spin_lock(ctx.lock);
    if (tail)
    {
        tail->next = node;
        node->prev = tail;
    }
    tail = node;
    node->bg = ctx.bg;
    node->fg = ctx.fg;
    spin_unlock(ctx.lock);
}

void lfbterm_restorecolor(void)
{
    struct cons_attr *node;
    node = tail;
    if (!node)
        return;
    spin_lock(ctx.lock);
    if (node->prev)
    {
        node->prev->next = 0;
        tail = node->prev;
        node->prev = 0;
    }
    else
        tail = 0;

    ctx.bg = node->bg;
    ctx.fg = node->fg;
    kfree(node);
    spin_unlock(ctx.lock);
}

void *lfbterm_cursor(void *arg __unused)
{
    int err = 0;
    if (use_gfx_cons == 0)
        thread_exit(-ENOENT);
    if ((err = lfb_console_init()))
        thread_exit(err);
    ctx_drawcursor(&ctx);
    loop();
}
BUILTIN_THREAD(lfb_text, lfbterm_cursor, NULL);