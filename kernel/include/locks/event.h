#pragma once

#include <ds/queue.h>
#include <lime/assert.h>
#include <locks/atomic.h>
#include <locks/spinlock.h>

typedef struct event
{
    atomic_t    event;
    queue_t     *queue;
    spinlock_t  *guard;
} event_t;

void event_free(event_t *__event);
int event_new(const char *__name, event_t **__event);

int event_wait(event_t *__event);
void event_signal(event_t *__event);
int event_broadcast(event_t *__event);