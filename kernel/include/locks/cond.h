#ifndef LOCKS_COND_H
#define LOCKS_COND_H

#include <ds/queue.h>
#include <lib/stdint.h>
#include <locks/spinlock.h>
#include <locks/atomic.h>

typedef struct cond
{
    char *name;
    spinlock_t *guard;
    atomic_t count;
    queue_t *waiters;
} cond_t;

int cond_init(cond_t *, char *, cond_t **);
int cond_free(cond_t *);
int cond_wait(cond_t *);

void cond_signal(cond_t *);
void cond_broadcast(cond_t *);
void cond_remove(cond_t *cond, thread_t *thread);

#endif // LOCKS_COND_H