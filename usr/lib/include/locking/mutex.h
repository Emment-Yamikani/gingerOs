#pragma once

#include <glist.h>
#include <types.h>
#include <locking/atomic.h>
#include <locking/spinlock.h>

typedef struct mutex
{
    glist_t list;
    atomic_t lock;
    tid_t threadID;
    spinlock_t guard;
} mutex_t;

#define __mutex_init() (&(mutex_t){ \
    .lock = 0,                      \
    .guard = {.flags = 0, .lock = 0, .thread = -1}, \
    .threadID = 0,                  \
    .list = {                       \
        .count = 0,                 \
        .head = NULL,               \
        .tail = NULL,               \
    },                              \
})

int mutex_init(mutex_t *mtx);
void mutex_lock(mutex_t *mtx);
void mutex_unlock(mutex_t *mtx);
int mutex_trylock(mutex_t *mtx);
int mutex_holding(mutex_t *mtx);