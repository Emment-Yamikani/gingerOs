#include <lib/stddef.h>
#include <bits/errno.h>
#include <mm/kalloc.h>
#include "lfbinternals.h"
#include <lib/string.h>

int lfb_initialize(int cols, int rows, textbuffer_t **pbuff) {
    int err = -ENOMEM;
    char *buffer = NULL;
    char **scanline = NULL;
    spinlock_t *lock = NULL;
    cond_t *cond_wait = NULL;
    textbuffer_t *txtbuff = NULL;

    if (!pbuff)
        return -EINVAL;

    if (!(txtbuff = kcalloc(1, sizeof *txtbuff)))
        goto error;

    if (!(buffer = kcalloc(3 * cols * rows, sizeof (char))))
        goto error;
    
    if (!(scanline = kcalloc(rows * 3, sizeof (char *))))
        goto error;

    if ((err = spinlock_init(NULL, "txtbuff", &lock)))
        goto error;
    
    if ((err = cond_init(NULL, "txtbuff", &cond_wait)))
        goto error;

    for (int row = 0; row < rows; ++row)
        scanline[row] = &buffer[row * cols];

    txtbuff->erow = 0;
    txtbuff->ecol = 0;
    txtbuff->cols = cols;
    txtbuff->rows = rows;
    txtbuff->lock = lock;
    txtbuff->buffer = buffer;
    txtbuff->wait = cond_wait;
    txtbuff->scanline = scanline;

    *pbuff = txtbuff;

    return 0;
error:
    if (cond_wait)
        cond_free(cond_wait);
    
    if (lock)
        spinlock_free(lock);

    if (scanline)
        kfree(scanline);

    if (buffer)
        kfree(buffer);

    if (txtbuff)
        kfree(txtbuff);

    return err;
}

#define __text_peek(buff, x, y)     ((buff)[(int)(y)][(int)(x)])
#define __text_setc(buff, x, y, c)  (__text_peek((buff), (x), (y)) = (c))
#define __text_putc(buff, mcols, mrows, x, y, c)  ({if (((x) < (int)mcols) && ((y) < (int)mrows)\
                                        && ((x) >= 0) && ((y) >= 0))\
                                            __text_setc((buff), (x), (y), (c)); })

#define __text_getc(buff, x, y)     ({if (((x) < (int)mcols) && ((y) < (int)mrows)\
                                        && ((x) >= 0) && ((y) >= 0))\
                                            int c = __text_peek((buff), (x), (y)); c; })

void lfb_textbuffer_flush(textbuffer_t *txtbuff);

void lfb_textbuffer_putc(textbuffer_t *txtbuff, int c) {
    textbuffer_assert_locked(txtbuff);
   
    if (txtbuff->ecol < 0)
        txtbuff->ecol = 0 ;
    
    if (txtbuff->erow < 0)
        txtbuff->erow = 0 ;
    
    __text_putc(txtbuff->scanline, txtbuff->cols, txtbuff->maxrows, txtbuff->ecol, txtbuff->erow, c);
    
    if (++txtbuff->ecol >= txtbuff->cols) {
        if (++txtbuff->erow >= txtbuff->maxrows) {
            for (int x = 0, y= 1, mr = txtbuff->maxrows; y < txtbuff->maxrows; ++y)
                for (; x < txtbuff->cols; ++x)
                    __text_putc(txtbuff->scanline, txtbuff->cols, mr, x, (y - 1), c);
            txtbuff->erow = (txtbuff->maxrows - 1);
            memset(&txtbuff->buffer[txtbuff->erow * txtbuff->cols], 0, txtbuff->cols);
        }
        txtbuff->ecol = 0;
    }
}
void lfb_textbuffer_putcat(textbuffer_t *txtbuff, int col, int row, int c);

int lfb_textbuffer_puts(textbuffer_t *txtbuff, const char *s);
int lfb_textbuffer_putsat(textbuffer_t *txtbuff, int col, int row, const char *s);