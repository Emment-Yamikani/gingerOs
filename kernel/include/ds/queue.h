#pragma once

#include <locks/spinlock.h>

struct queue;

typedef struct queue_node
{
    void *data;
    struct queue *queue;
    struct queue_node *prev, *next;
} queue_node_t;

typedef struct queue
{
    int count;
    int flags;
    char *name;
    spinlock_t *lock;
    queue_node_t *head, *tail;
} queue_t;

#define QUEUE_NEW(nam)                                                                           \
    &(queue_t) {.count = 0, .name = nam, .flags = 0, .lock = SPINLOCK_NEW(nam), .head = 0, .tail = 0 }

// lock a queue
#define queue_lock(q)      \
{                          \
    assert(q, "no queue"); \
    spin_lock(q->lock);    \
}

// unlock a queue
#define queue_unlock(q)    \
{                          \
    assert(q, "no queue"); \
    spin_unlock(q->lock);  \
}

#define queue_assert(q) assert(q, "no queue");

#define queue_assert_lock(q) spin_assert_lock(q->lock)

void *dequeue(queue_t *);

void queue_free(queue_t *);

void queue_flush(queue_t *);

int queue_count(queue_t *);

int queue_new(const char *, queue_t **);

queue_node_t *enqueue(queue_t *, void *);

int queue_remove(queue_t *, void *);

queue_node_t *queue_contains(queue_t *, void *);

int queue_contains_node(queue_t *q, queue_node_t *node);

int queue_remove_node(queue_t *, queue_node_t *);