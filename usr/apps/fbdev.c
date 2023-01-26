#include <ginger.h>
#include <fbterm/fb.h>
#include <fbterm/tinyfont.h>

//#define _NJ_INCLUDE_HEADER_ONLY
#include "nanojpeg.c"

#define DEFAULT_FONT "/mnt/ramfs/font.tf"
#define DEFAULT_WALL "/mnt/ramfs/wall.jpg"
char *wallpaper_path = DEFAULT_WALL;


int fbx_fd = -1;
int kbd_fd = -1;
int font_fd = -1;
int uart_fd = -1;
int bpp = 0, xres = 0, yres = 0, line_length = 0;

struct fb_fixinfo fix_info;
struct fb_varinfo var_info;

typedef struct ctx
{
    uint8_t opacity;
    struct font *font;
    uint8_t *textbuff;
    uint8_t *backbuff;
    uint8_t *wallpaper;
    int ctx_x, ctx_y;
} ctx_t;

int fbx_render(ctx_t *ctx)
{
    lseek(fbx_fd, 0, SEEK_SET);
    write(fbx_fd, ctx->backbuff, yres * line_length);
    return 0;
}

#define COLORMERGE(f, b, c) ((b) + (((f) - (b)) * (c) >> 8u))
// #define _R(c) (((c) >> 3 * 8) & 0xFF)
// #define _G(c) (((c) >> 2 * 8) & 0xFF)
// #define _B(c) (((c) >> 1 * 8) & 0xFF)
// #define _A(c) (((c) >> 0 * 8) & 0xFF)

#define _pick_color_cell(p, i) (((p) >> i * 8) & 0xff)
#define _R(c) _pick_color_cell(c, 3)
#define _G(c) _pick_color_cell(c, 2)
#define _B(c) _pick_color_cell(c, 1)
#define _A(c) _pick_color_cell(c, 0)

void fb_put_pixel(struct fbterm_ctx *ctx, int x, int y, uint32_t fg, uint32_t bg)
{
    unsigned char a = _A(fg);
    unsigned char *p = &ctx->textbuf[y * line_length + bpp * x];
    p[0] = COLORMERGE(_R(fg), _R(bg), a); /* Red */
    p[1] = COLORMERGE(_G(fg), _G(bg), a); /* Green */
    p[2] = COLORMERGE(_B(fg), _B(bg), a); /* Blue */

    unsigned char *q = &ctx->backbuf[y * line_length + bpp * x];
    q[0] = COLORMERGE(p[0], q[0], ctx->op); /* Red */
    q[1] = COLORMERGE(p[1], q[1], ctx->op); /* Green */
    q[2] = COLORMERGE(p[2], q[2], ctx->op); /* Blue */
}

#define __color_primer(p, i) (((uint32_t)p) << i * 8)
#define rRGB(p) __color_primer(p[2], 2)
#define gRGB(p) __color_primer(p[1], 1)
#define bRGB(p) __color_primer(p[0], 0)

void fb_get_pixel(uint8_t *buf, int x, int y, uint32_t *color)
{
    uint8_t *p = &buf[y * line_length + bpp * x];
    *color = rRGB(p) | gRGB(p) | bRGB(p);
}

static void fbterm_putc(struct fbterm_ctx *ctx, int col, int row, int c, uint32_t fg, uint32_t bg)
{
    struct font *font = ctx->font;

    char fbuf[font->rows * font->cols];
    font_bitmap(font, fbuf, c);

    for (int i = 0; i < font->rows; ++i)
    {
        int cx = col * font->cols;
        for (int j = 0; j < font->cols; ++j)
        {
            char v = fbuf[i * font->cols + j];
            // fb_get_pixel(ctx->wallpaper, cx, row * font->rows + i, &bg);
            fb_put_pixel(ctx, cx, row * font->rows + i, _ALPHA(fg, v), bg);
            ++cx;
        }
    }
}

void fbterm_set_cursor(struct fbterm_ctx *ctx, int row, int col)
{
    ctx->cr = row;
    ctx->cc = col;
}

void fb_clear(struct fbterm_ctx *ctx)
{
    memset(ctx->textbuf, 0, yres * line_length);
    memset(ctx->backbuf, 0, yres * line_length);

    if (ctx->wallpaper)
        memcpy(ctx->backbuf, ctx->wallpaper, yres * line_length);
}

int fb_cook_wallpaper(struct fbterm_ctx *ctx, char *path)
{
    ctx->font = 0;
    int img = open(path, O_RDONLY);
    size_t size = lseek(img, 0, SEEK_END);
    lseek(img, 0, SEEK_SET);

    char *buf = calloc(1, size);
    read(img, buf, size);
    close(img);

    int err = 0;

    ctx->wallpaper = calloc(1, yres * line_length);

    njInit();

    if (err = njDecode(buf, size))
    {
        free(buf);
        // fprintf(stderr, "Error decoding input file: %d\n", err);
        printf("Error decoding input file: %d\n", err);
        return -1;
    }

    free(buf);
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

#define WP_POS(i, j) (((i) + ypan) * line_length + ((j) + xpan) * bpp)
#define IMG_POS(i, j) (((i) + yoffset) * img_line_length + ((j) + xoffset) * ncomp)

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
    memcpy(ctx->backbuf, ctx->wallpaper, yres * line_length);

    return 0;
}


int fbterm_init(struct fbterm_ctx *ctx)
{
    memset(ctx, 0, sizeof(struct fbterm_ctx));
    fb_term_init(ctx);
    fb_cook_wallpaper(ctx, wallpaper_path);
    return 0;
}

void fb_term_init(struct fbterm_ctx *ctx)
{
    printf("fbterm init continuing...\n");
    ctx->op = 200;
    ctx->rows = yres / ctx->font->rows;
    ctx->cols = xres / ctx->font->cols;
    ctx->textbuf = calloc(1, yres * line_length);
    ctx->backbuf = calloc(1, yres * line_length);
}

size_t fbterm_clear(struct fbterm_ctx *ctx)
{
    fb_clear(ctx);
}

void fbterm_cursor_draw(struct fbterm_ctx *ctx, int row, int col)
{
    struct font *font = ctx->font;
    for (int i = font->rows - 1; i < font->rows; ++i)
    {
        int cx = col * font->cols;
        for (int j = 0; j < font->cols; ++j)
        {
            fb_put_pixel(ctx, cx, row * font->rows + i, -1, 0);
            ++cx;
        }
    }
}

int fbx_redraw(ctx_t *ctx)
{
    memcpy(ctx->backbuff, ctx->wallpaper, yres * line_length); // draw the wallpaper
    // draw all object in z-order
    // draw mouse cursor
    fbx_render(ctx); // render ther final image
}

int fbx_init(char *path)
{
    fbx_fd = open(path, O_RDWR);
    if (fbx_fd < 0)
        return -1;

    if (ioctl(fbx_fd, FBIOGET_FIX_INFO, &fix_info) < 0)
    {
        close(fbx_fd);
        fbx_fd = -1;
        return -1;
    }

    if (ioctl(fbx_fd, FBIOGET_VAR_INFO, &var_info) < 0)
    {
        close(fbx_fd);
        fbx_fd = -1;
        return -1;
    }

    xres = var_info.width;
    yres = var_info.height;
    bpp = var_info.bpp / 8;
    line_length = fix_info.line_length;

    return 0;
}

int fbx_cook_wallpaper(ctx_t *ctx, char *path)
{
    int img = open(path, O_RDONLY);
    size_t size = lseek(img, 0, SEEK_END);
    lseek(img, 0, SEEK_SET);

    char *buf = malloc(size);
    read(img, buf, size);
    close(img);

    int err = 0;

    // njInit();
    if (err = njDecode(buf, size))
    {
        free(buf);
        // fprintf(stderr, "Error decoding input file: %d\n", err);
        printf("Error decoding input file: %d\n", err);
        return -1;
    }

    free(buf);
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

#define WP_POS(i, j) (((i) + ypan) * line_length + ((j) + xpan) * bpp)
#define IMG_POS(i, j) (((i) + yoffset) * img_line_length + ((j) + xoffset) * ncomp)

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
    memcpy(ctx->backbuff, ctx->wallpaper, yres * line_length);

    return 0;
}

void *vterm_alloc(size_t size)
{
    return malloc(size);
}

struct font font;

int vterm_init(ctx_t *ctx)
{
    ctx->opacity = 200;
    ctx->ctx_x = xres / ctx->font->cols;
    ctx->ctx_y = yres / ctx->font->rows;

    ctx->backbuff = (uint8_t *)vterm_alloc(yres * line_length);
    ctx->textbuff = (uint8_t *)vterm_alloc(yres * line_length);
    ctx->wallpaper = (uint8_t *)vterm_alloc(yres * line_length);

    memset(ctx->wallpaper, 0x4E, yres * line_length);
    fbx_cook_wallpaper(ctx, wallpaper_path);
    return 0;
}

static void *xread(int fd, int len)
{
    void *buf = calloc(1, len);
    if (buf && read(fd, buf, len) == len)
        return buf;
    free(buf);
    return NULL;
}

struct font *font_open(char *path)
{
    struct font *font;
    struct tinyfont head = {0};
    int fd = open(path, O_RDONLY);

    lseek(fd, 0, SEEK_SET);

    if (fd < 0 || (size_t)read(fd, &head, sizeof(head)) != sizeof(head))
    {
        close(fd);
        return NULL;
    }

    font = calloc(1, sizeof(*font));

    font->n = head.n;
    font->rows = head.rows;
    font->cols = head.cols;
    font->glyphs = xread(fd, font->n * sizeof(int));
    font->data = xread(fd, font->n * font->rows * font->cols);

    close(fd);

    if (!font->glyphs || !font->data)
    {
        font_free(font);
        return NULL;
    }

    return font;
}

static int find_glyph(struct font *font, int c)
{
    int l = 0;
    int h = font->n;
    while (l < h)
    {
        int m = (l + h) / 2;
        if (font->glyphs[m] == c)
            return m;
        if (c < font->glyphs[m])
            h = m;
        else
            l = m + 1;
    }
    return -1;
}

int font_bitmap(struct font *font, void *dst, int c)
{
    int i = find_glyph(font, c);
    int len = font->rows * font->cols;
    if (i < 0)
        return 1;
    memcpy(dst, font->data + i * len, len);
    return 0;
}

void font_free(struct font *font)
{
    if (font->data)
        free(font->data);
    if (font->glyphs)
        free(font->glyphs);
    free(font);
}

int font_rows(struct font *font)
{
    return font->rows;
}

int font_cols(struct font *font)
{
    return font->cols;
}

int main(int argc, char *argv[])
{
    close(0);
    close(1);
    close(2);

    if (argc > 1)
        wallpaper_path = argv[1];

    kbd_fd = open("/dev/kbd0", O_RDWR);
    dup2((uart_fd = open("/dev/uart", O_WRONLY)), 2);

    if (fbx_init("/dev/fbdev") < 0)
    {
        printf("error opening framebuffer\n");
        exit(1);
    }

    ctx_t ctx; // = vterm_alloc(sizeof (ctx_t));
    if (!(ctx.font = font_open(DEFAULT_FONT)))
    {
        printf("error openning tinyfont database\n");
        exit(1);
    }

    font = *ctx.font;
    vterm_init(&ctx);
    *ctx.font = font;
    

    fbx_redraw(&ctx);
    return 0;
}