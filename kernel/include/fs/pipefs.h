#pragma once

#include <fs/fs.h>
#include <locks/cond.h>
#include <ds/ringbuf.h>

typedef struct pipe
{
    int ropen; /*read end is open?*/
    int wopen; /*write end is open?*/
    cond_t *readers; /*readers wait queue*/
    cond_t *writers; /*writers queue*/
    spinlock_t *lock; /*pipe lock*/
    ringbuf_t *ringbuf; /*pipe circular buffer*/
} pipe_t;


#define PIPESZ  512 /*max allowed buffer size*/

int pipefs_init(void);
int pipefs_pipe(file_t *f0, file_t *f1);
int pipefs_pipe_raw(inode_t **read, inode_t **write);