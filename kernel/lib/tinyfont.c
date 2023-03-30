#include <lib/string.h>
#include <mm/kalloc.h>
#include <fs/fs.h>
#include <fs/sysfile.h>
#include <font/tinyfont.h>

static void *xread(inode_t *file, off_t off, size_t len)
{
    void *buf = kcalloc(1, len);
    if (buf && (iread(file, off, buf, len) == len))
        return buf;
    kfree(buf);
    return NULL;
}

struct font *font_open(char *path)
{
    struct font *font;
    struct tinyfont head = {0};
    uio_t uio = {0};
    inode_t *file = NULL;
    vfs_open(path, &uio, O_RDONLY, &file);
    
    if ((size_t)iread(file, 0, &head, sizeof(head)) != sizeof(head))
    {
        iclose(file);
        return NULL;
    }

    font = kcalloc(1, sizeof(*font));

    font->n = head.n;
    font->rows = head.rows;
    font->cols = head.cols;
    font->glyphs = xread(file, sizeof(head), font->n * sizeof(int));
    font->data = xread(file, (sizeof(head) + font->n * sizeof(int)), font->n * font->rows * font->cols);

    iclose(file);

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
        kfree(font->data);
    if (font->glyphs)
        kfree(font->glyphs);
    kfree(font);
}

int font_rows(struct font *font)
{
    return font->rows;
}

int font_cols(struct font *font)
{
    return font->cols;
}