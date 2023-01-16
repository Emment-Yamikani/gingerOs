#pragma once
#include <lib/stdint.h>
#include <locks/spinlock.h>
#include <locks/atomic.h>
#include <ds/queue.h>
#include <arch/i386/cpu.h>
#include <sys/sched.h>
#include <lime/assert.h>
#include <lib/stddef.h>

typedef struct mutex
{
    atomic_t lock;
    queue_t *waiters;
    thread_t *thread;
    spinlock_t *guard;
} mutex_t;

extern tid_t thread_self(void);

#define mutex_holding(mutex) (mutex->thread == current)
#define mutex_assert(mutex) assert(mutex, "no mutex");
#define mutex_assert_lock(mutex)                                                                                                    \
    {                                                                                                                               \
        if (!mutex_holding(mutex))                                                                                                  \
            panic("%s:%d:  %p =? %p thread(%d) not holding mutex(%p) return [\e[0;013m%p\e[0m]\n", __FILE__, __LINE__, current, mutex->thread, thread_self(), mutex, return_address(0)); \
    }

int mutex_lock(mutex_t *);
void mutex_free(mutex_t *);
void mutex_unlock(mutex_t *);
int mutex_try_lock(mutex_t *);
int mutex_init(mutex_t *, mutex_t **);
void mutex_remove(mutex_t *mutex, thread_t *thread);