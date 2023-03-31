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
    int op, fg, bg;
    uint8_t *txtbuf;
    struct font *font;
    uint32_t *lfb_back;
    uint32_t **scanline;
    uint8_t cursor_char;
    uint8_t cursor_timeout;
    int cols, rows, cc, cr;
    uint8_t *wallpaper_data;
    spinlock_t *lock;
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
                                     __set_pixel((ctx)->scanline, (x), (y), (c));})

int lfbterm_init(void)
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

    if ((err = spinlock_init(NULL, "lfbterm", &ctx.lock)))
        goto error;

    err = -ENOMEM;
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

    ctx.cursor_char = '_';
    ctx.cursor_timeout = 35;
    ctx.op = 200;
    ctx.bg = RGB_black;
    ctx.bg = 0x002B36;//RGB_black;
    ctx.fg = RGB_cadet_blue;
    ctx.cols = fbvar.width / ctx.font->cols;
    ctx.rows = fbvar.height / ctx.font->rows;

    use_gfx_cons = 1;
    lfbterm_clrscrn();
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

int cxx = 0;
void ctx_drawcursor(struct lfb_ctx *ctx)
{
    jiffies_t now __unused = jiffies_get();
    loop() {
        spin_lock(ctx->lock);
        // set
        font_putc(ctx->cursor_char, ctx, ctx->cc, ctx->cr);
        spin_unlock(ctx->lock);
        //now = jiffies_get();
        wait_ms(ctx->cursor_timeout);//sleep(1);
        //now = jiffies_get();
        // clear
        spin_lock(ctx->lock);
        font_putc(' ', ctx, ctx->cc, ctx->cr);
        spin_unlock(ctx->lock);
        wait_ms(ctx->cursor_timeout);//sleep(1);
    }
}

void lfbterm_scroll(void);

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
        font_putc(' ', ctx, ctx->cc, ctx->cr);
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
        lfbterm_scroll();
}

void ctx_puts(struct lfb_ctx *ctx, char *str)
{
    while (*str)
        ctx_putchar(ctx, *str++);
}

void lfbterm_putc(int c)
{
    cxx = c;
    switch(c)
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
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if ((w < 0) || (h < 0)) return;

    int xmax = w + x, ymax = h + y;
    if (xmax > (int)fbvar.width)
        xmax = fbvar.width;
    if (ymax > (int)fbvar.height)
        ymax = fbvar.height;

    for (; y < ymax; ++y)
        for (int cx = x; cx < xmax; ++cx)
            __put_pixel((&ctx), cx, y, color);
}

void lfbterm_clrscrn(void)
{
    spin_lock(ctx.lock);
    lfbterm_fill_rect(0, 0, fbvar.width, fbvar.height, ctx.bg);
    ctx.cc = ctx.cr = 0;
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
    for (int row = ctx.font->rows; row < (int)fbvar.height; ++row)
        for (int col = 0; col < (int)fbvar.width; ++col)
            __put_pixel((&ctx), col, (row - ctx.font->rows), __peek_pixel(ctx.scanline, col, row));
    for (int row = (int)fbvar.height - ctx.font->rows; row < (int)fbvar.height; ++row)
        for (int col = 0; col < (int)fbvar.width; ++col)
            __put_pixel((&ctx), col, row, ctx.bg);
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