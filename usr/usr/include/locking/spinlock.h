#pragma once

#include <types.h>
#include <stddef.h>
#include <stdint.h>
#include <locking/atomic.h>
#include <ginger/assert.h>
#include <ginger/barrier.h>
#include <unistd.h>

/**
 * @brief spinlock object
 *
 */
typedef struct spinlock
{
    atomic_t lock;
    tid_t thread;
    int flags;
} spinlock_t;

#ifndef return_address
#define return_address(l) __builtin_return_address(l)
#endif

#define SPINLOCK_NEW()                               \
    &(spinlock_t)                                     \
    {                                                 \
        .thread = 0, .flags = 0, .lock = 0 \
    }

#define __spinlock_init() (spinlock_t){.flags = 0, .lock = 0, .thread = -1}

/**
 * @brief initialize a spinlock
 *
 * @param __lock
 * @param __name
 * @param __ref
 * @return int
 */
#define spinlock_init(__lock) ({int err = 0; if ((__lock))*(__lock) = __spinlock_init(); else err = -EINVAL; err;})

/**
 * @brief free a spinlock object
 *
 * @param __lock
 */
void spinlock_free(spinlock_t *__lock);

#define spin_holding(__lock) \
    (((__lock)->thread == thread_self()) && atomic_read(&(__lock)->lock))

/**
 * @brief acquire a spinlock
 *
 * @param __lock
 */
#define spin_lock(__lock)                                                                  \
    {                                                                                      \
        assert((__lock), "no lock");                                                         \
        if (spin_holding((__lock)))                                                          \
            panic("thread%d: %s:%d: in %s() : holding spinlock[%p] return [%p]\n",                 \
                  thread_self(), __FILE__, __LINE__, __func__, (__lock), return_address(0)); \
        barrier();                                                                         \
        while (atomic_xchg(&(__lock)->lock, 1) == 1)                                         \
            CPU_RELAX();                                                                   \
        (__lock)->thread = thread_self();                                                    \
    }

/**
 * @brief release a spinlock
 *
 * @param __lock
 */
#define spin_unlock(__lock)                                                                \
    {                                                                                      \
        assert((__lock), "no lock");                                                         \
        if (!spin_holding((__lock)))                                                         \
            panic("thread%d: %s:%d: in %s() : not holding spinlock[%p] return [%p]\n",  \
                  thread_self(), __FILE__, __LINE__, __func__, (__lock), return_address(0)); \
        barrier();                                                                         \
        (__lock)->thread = 0;                                                                \
        atomic_write(&(__lock)->lock, 0);                                                    \
    }

/**
 * @brief try to acquire a spinlock
 *
 * @param __lock
 * @return int '1' if acquisition was successful(lock wasn't held before) and '0' if acquisition failed(lock is already held)
 */
static inline int spin_try_lock(spinlock_t *__lock)
{
    assert(__lock, "no spinlock");
    barrier();
    long held = !atomic_xchg(&__lock->lock, 1);
    if (held)
        __lock->thread = thread_self();
    return held; // not the one's who locked it?
}

#define spin_assert_lock(lk) if (!spin_holding(lk)) panic("%s:%d: thread%d: caller must hold spinlock[%p], return [%p]\n", __FILE__, __LINE__, thread_self(), (lk), return_address(0));