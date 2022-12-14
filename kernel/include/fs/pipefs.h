#pragma once

#include <fs/fs.h>
#include <locks/cond.h>
#include <ds/ringbuf.h>

typedef struct pipe
{
    int ropen; /*read end is open?*/
    int wopen; /*write end is open?*/
    cond_t *readers;
    cond_t *writers;
    spinlock_t *lock; /*pipe lock*/
    ringbuf_t *ringbuf; /*pipe circular buffer*/
} pipe_t;

#define PIPESZ  512 /*max allowed buffer size*/

int pipefs_init(void);
int pipefs_pipe(file_t *f0, file_t *f1);