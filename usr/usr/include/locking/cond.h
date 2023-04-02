#pragma once

#include <locking/mutex.h>

typedef struct _cv
{
    glist_t queue;
    atomic_t event;
    mutex_t guard;
} cv_t;

#define __cv_init() (&(cv_t){ \
    .event = 0,               \
    .queue = {                \
        .count = 0,           \
        .head = NULL,         \
        .tail = NULL,         \
    },                        \
    .guard = {                \
        .lock = 0,            \
        .guard = 0,           \
        .threadID = 0,        \
        .list = {             \
            .count = 0,       \
            .head = NULL,     \
            .tail = NULL,     \
        },                    \
    }})

int cv_init(cv_t *);
int cv_wait(cv_t *);
void cv_signal(cv_t *);
void cv_broadcast(cv_t *);