#include <ginger.h>
#include <gfx/gfx.h>
#include <fbterm/fb.h>

static int fb = 0;
static int bpp = 0;
static int line_length = 0;
static size_t framebuffer_size = 0;

static tid_t thread[10] = {0};

struct fbterm_ctx ctx;
static fb_fixinfo_t fix = {0};
static fb_varinfo_t var = {0};
static struct font *font = NULL;

/*function proto-types*/
void *renderer(void *arg);

int init_ctx(struct fbterm_ctx *ctx, struct font *font)
{
    if (ctx == NULL)
        panic("NULL ctx\n");

    framebuffer_size = line_length * var.height;
    
    ctx->backbuf = malloc(framebuffer_size);
    if (ctx->backbuf == NULL)
        panic("couldn't allocate backbuffer, no memory\n");

    ctx->wallpaper = malloc(framebuffer_size);
    if (ctx->wallpaper == NULL)
        panic("couldn't allocate wallpaper, no memory\n");

    ctx->textbuf = malloc(framebuffer_size);
    if (ctx->textbuf == NULL)
        panic("couldn't allocate text, no memory\n");

    lseek(fb, 0, SEEK_SET);
    read(fb, ctx->wallpaper, framebuffer_size);
    lseek(fb, 0, SEEK_SET);

    memcpy(ctx->backbuf, ctx->wallpaper, framebuffer_size);
    memcpy(ctx->textbuf, ctx->wallpaper, framebuffer_size);

    ctx->op = 200;
    
    ctx->cc = 0;
    ctx->cr = 0;
    
    ctx->cols = var.width / font->cols;
    ctx->rows = var.height / font->rows;

    ctx->font = font;
    ctx->txt_fg = RGB_papaya_whip;

    return 0;
}

void put_pixel(struct fbterm_ctx *dst_ctx, double x, double y, int pixel)
{
    // printk("(%4.1f, %4.1f)\n", x, y);

    if ((x >= (double)var.width) || (x < 0.0))
        return;
    if ((y >= (double)var.height) || (y < 0.0))
        return;
    int location = (x * (double)(var.bpp / 8)) + (double)(y * (double)fix.line_length);
    *(uint32_t *)(dst_ctx->textbuf + location) = BLEND_RGBA(pixel, PICK_PIXEL((uint32_t *)(dst_ctx->wallpaper + location)), dst_ctx->op);
}

int font_putc(int c, struct fbterm_ctx *ctx, int col, int row, int fg)
{
    char glyph[font->rows * font->cols];
    font_bitmap(font, glyph, c);
    for (int i = 0; i < font->rows; ++i)
    {
        int cx = col * font->cols;
        for (int j = 0; j < font->cols; ++j)
        {
            char v = glyph[i * font->cols + j];
            for (int b = 0; b < 8; ++b)
                if (v & (1 << b))
                    put_pixel(ctx, cx, row * font->rows + i, fg);
            ++cx;
        }
    }
}

void ctx_putchar(struct fbterm_ctx *ctx, int c);

void ctx_drawcursor(struct fbterm_ctx *ctx){
}

void ctx_putchar(struct fbterm_ctx *ctx, int c)
{
    if (c == '\n'){
        ctx->cc = 0;
        ctx->cr++;
    }else if (c == '\t'){
        ctx->cc = (ctx->cc + 4) & ~3;
        if (ctx->cc >= ctx->cols){
            ctx->cr++;
            ctx->cc = 0;
        }
    }else if (c == '\r'){
        ctx->cc = 0;
    }else if (c == '\b'){
        ctx->cc--;
        if (ctx->cc == (ctx->cols - 1))
            ctx->cr--;
    }else if (c >= ' '){
        font_putc(' ', ctx, ctx->cc, ctx->cr, ctx->txt_fg);
        font_putc(c, ctx, ctx->cc, ctx->cr, ctx->txt_fg);
        ctx->cc++;
        if (ctx->cc >= ctx->cols)
            ctx->cr++;
        ctx_drawcursor(ctx);
    }
}

void ctx_puts(struct fbterm_ctx *ctx, char *str)
{
    while(*str) ctx_putchar(ctx, *str++);
}

void render(struct fbterm_ctx *ctx)
{
    if (ctx == NULL)
        panic("error, NULL ctx\n");
    
    memcpy(ctx->backbuf, ctx->textbuf, framebuffer_size);
    lseek(fb, 0, SEEK_SET);
    write(fb, ctx->backbuf, framebuffer_size);
    lseek(fb, 0, SEEK_SET);
}


int main (int argc, const char *argv[]){
    void *thread_ret = NULL;

    font = font_open("/font.tf");

    fb = open("/dev/fbdev", O_RDWR);
    
    lseek(fb, 0, SEEK_SET);
    ioctl(fb, FBIOGET_FIX_INFO, &fix);
    ioctl(fb, FBIOGET_VAR_INFO, &var);
    line_length = var.pitch;
    bpp = var.bpp;


    init_ctx(&ctx, font);

    thread_create(&thread[0], renderer, NULL);


    char cwd[100];
    getcwd(cwd, sizeof cwd);

    char *str = malloc(510);
    memset(str, 0, 510);

    snprintf(str, 510, "Hello, World\nWe are finally here\n[font %s]$ ", cwd);

    ctx_puts(&ctx, str);
    

    while(1)
    {
        int c = 0;
        read(0, &c, 1);
        ctx_putchar(&ctx, c);
    }

    thread_join(thread[0], &thread_ret);
    

    return 0;
}

void *renderer(void *arg)
{
    (void)arg;
    ctx_drawcursor(&ctx);
    while (1)
    {
        render(&ctx);
    }
    
}