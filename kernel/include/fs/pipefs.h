#pragma once

#include <fs/fs.h>
#include <locks/cond.h>
#include <ds/ringbuf.h>

typedef struct pipe
{
    int read_open;
    int write_open;
    cond_t *readers;
    cond_t *writers;
    spinlock_t *lock;
    ringbuf_t *ringbuf;
} pipe_t;

#define PIPESZ  512

int pipefs_pipe(file_t *f0, file_t *f1);