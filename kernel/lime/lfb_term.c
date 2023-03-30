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

typedef struct lfb_ctx
{
    int op, fg, bg;
    uint8_t *txtbuf;
    struct font *font;
    uint32_t *lfb_back;
    uint32_t **scanline;
    int cols, rows, cc, cr;
    uint8_t *wallpaper_data;
} lfb_ctx_t;

lfb_ctx_t ctx;
fb_fixinfo_t fbfix;
fb_varinfo_t fbvar;
inode_t *lfb = NULL;
inode_t *lfb_img = NULL;
volatile int use_gfx_cons = 0;

const char *wallpaper_path[] = {
    "cyan_800x600.jpg",
    "cyan_1024x768.jpg",
    NULL,
};


#define __peek_pixel(f, x, y) ((f)[(int)(y)][(int)(x)])
#define __set_pixel(f, x, y, c) (__peek_pixel((f), (x), (y)) = (c))
#define __put_pixel(ctx, x, y, c) ({if (((x) < (int)fbvar.width) &&\
                                     ((y) < (int)fbvar.height) && ((x) >= 0) && ((y) >= 0))\
                                     __set_pixel(ctx->scanline, (x), (y), (c));})

int lfb_term_init(void)
{
    uio_t uio = {0};
    int err = -ENOMEM;
    if ((err = vfs_open("/dev/fbdev", &uio, O_RDWR, &lfb)))
    {
        printk("%s:%d: error, fallback to text_mode\n", __FILE__, __LINE__);
        return err;
    }

    iioctl(lfb, FBIOGET_FIX_INFO, &fbfix);
    iioctl(lfb, FBIOGET_VAR_INFO, &fbvar);

    memset(&ctx, 0, sizeof ctx);
    if (!(ctx.scanline = kcalloc(fbvar.height, (fbvar.bpp / 8))))
        goto error;
    if (!(ctx.lfb_back = kcalloc(1, fbfix.memsz)))
        goto error;
    if (!(ctx.txtbuf = kcalloc(1, fbfix.memsz)))
        goto error;
    ctx.lfb_back = (void *)fbfix.addr;
    
    err = -EINVAL;
    if (!(ctx.font = font_open("/font.tf")))
        goto error;

    for (size_t row = 0; row < fbvar.height; row++)
        ctx.scanline[row] = &((uintptr_t *)ctx.lfb_back)[row * fbvar.width];

    ctx.op = 200;
    ctx.bg = RGB_black;
    ctx.fg = RGB_white_smoke;
    ctx.cols = fbvar.width / ctx.font->cols;
    ctx.rows = fbvar.height / ctx.font->rows;

    use_gfx_cons = 1;
    return 0;
error:
    if (ctx.txtbuf)
        kfree(ctx.txtbuf);
    if (ctx.lfb_back)
        kfree(ctx.lfb_back);
    if (ctx.scanline)
        kfree(ctx.scanline);
    return err;
}

int font_putc(int c, struct lfb_ctx *ctx, int col, int row)
{
    char glyph[ctx->font->rows * ctx->font->cols];
    font_bitmap(ctx->font, glyph, c);
    for (int i = 0; i < ctx->font->rows; ++i)
    {
        int cx = col * ctx->font->cols;
        for (int j = 0; j < ctx->font->cols; ++j)
        {
            char v = glyph[i * ctx->font->cols + j];
            for (int b = 0; b < 8; ++b)
                __put_pixel(ctx, cx, (row * ctx->font->rows + i), ((v & (1 << b)) ? ctx->fg : ctx->bg));
            ++cx;
        }
    }
    return 0;
}

void ctx_putchar(struct lfb_ctx *ctx, int c);

void ctx_drawcursor(struct lfb_ctx *ctx)
{
    (void)ctx;
    jiffies_t now __unused = jiffies_get();
    loop(){
        // set
        font_putc('|', ctx, ctx->cc, ctx->cr);
        //now = jiffies_get();
        sleep(1);
        //now = jiffies_get();
        // clear
        font_putc(' ', ctx, ctx->cc, ctx->cr);
        sleep(1);
    }
}

void lfb_term_scroll(void);

void ctx_putchar(struct lfb_ctx *ctx, int c)
{
    switch(c)
    {
    case'\n':
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
        if (ctx->cc < 0) {
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
        if (ctx->cc >= ctx->cols) {
            ctx->cr++;
            ctx->cc = 0;
        }
    }
    if (ctx->cr >= ctx->rows)
        lfb_term_scroll();
}

void lfb_term_putc(int c)
{
    ctx_putchar(&ctx, c);
}

void ctx_puts(struct lfb_ctx *ctx, char *str)
{
    while (*str)
        ctx_putchar(ctx, *str++);
}

int lfb_term_puts(const char *s)
{
    char *S = (char *)s;
    while (*S)
        ctx_putchar(&ctx, *S++);
    return S - s;
}

void lfb_term_scroll(void)
{
    for (int row = ctx.font->rows; row < (int)fbvar.height; ++row)
        for (int col = 0; col < (int)fbvar.width; ++col)
            __put_pixel((&ctx), col, (row - ctx.font->rows), __peek_pixel(ctx.scanline, col, row));
    for (int row = (int)fbvar.height - ctx.font->rows; row < (int)fbvar.height; ++row)
        for (int col = 0; col < (int)fbvar.width; ++col)
            __put_pixel((&ctx), col, row, ctx.bg);
    ctx.cc = 0;
    ctx.cr = ctx.rows - 1;
}

void *lfb_writer(void *arg __unused)
{
    if (use_gfx_cons == 0)
        thread_exit(-ENOENT);
    ctx_drawcursor(&ctx);
    loop();
}
BUILTIN_THREAD(lfb_text, lfb_writer, NULL);