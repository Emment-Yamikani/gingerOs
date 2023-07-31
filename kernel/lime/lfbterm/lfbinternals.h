#pragma once
#include <locks/cond.h>
#include <lime/assert.h>
#include <locks/spinlock.h>

typedef struct textbuffer
{
    int flags;
    char *buffer;
    int cols, rows;
    int ecol, erow;
    int maxrows;
    char **scanline;
    cond_t *wait;
    spinlock_t *lock;
} textbuffer_t;

#define textbuffer_assert(tb) ({assert(tb, "No textbuff"); })
#define textbuffer_lock(tb) ({textbuffer_assert(tb); spin_lock(tb->lock); })
#define textbuffer_unlock(tb) ({textbuffer_assert(tb); spin_unlock(tb->lock); })
#define textbuffer_assert_locked(tb) ({textbuffer_assert(tb); spin_assert_lock(tb->lock); })

int lfb_initialize(int cols, int rows, textbuffer_t **pbuff);

void lfb_textbuffer_flush(textbuffer_t *txtbuff);

void lfb_textbuffer_putc(textbuffer_t *txtbuff, int c);
void lfb_textbuffer_putcat(textbuffer_t *txtbuff, int col, int row, int c);

int lfb_textbuffer_puts(textbuffer_t *txtbuff, const char *s);
int lfb_textbuffer_putsat(textbuffer_t *txtbuff, int col, int row, const char *s);