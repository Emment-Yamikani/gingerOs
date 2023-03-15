#pragma once

#include <arch/i386/cpu.h>
#include <lib/stddef.h>
#include <lib/stdint.h>
#include <locks/atomic.h>
#include <lime/preempt.h>
#include <lime/assert.h>
#include <locks/barrier.h>

/**
 * @brief spinlock object
 *
 */
typedef struct spinlock
{
    atomic_t lock;
    cpu_t *cpu;
    char *name;
    int flags;
} spinlock_t;

#define SPINLOCK_NEW(s)                               \
    &(spinlock_t)                                     \
    {                                                 \
        .cpu = NULL, .flags = 0, .name = s, .lock = 0 \
    }

/**
 * @brief initialize a spinlock
 *
 * @param __lock
 * @param __name
 * @param __ref
 * @return int
 */
int spinlock_init(const spinlock_t *__lock, const char *__name, spinlock_t **__ref);

/**
 * @brief free a spinlock object
 *
 * @param __lock
 */
void spinlock_free(spinlock_t *__lock);

#define spin_holding(__lock) \
    ((__lock->cpu == cpu) && atomic_read(&__lock->lock))

/**
 * @brief acquire a spinlock
 *
 * @param __lock
 */
#define spin_lock(__lock)                                                                  \
    {                                                                                      \
        assert(__lock, "no lock");                                                         \
        pushcli();                                                                         \
        if (spin_holding(__lock))                                                          \
            panic("cpu%d: %s:%d: in %s() : holding '%s' return [\e[0;013m%p\e[0m]\n",      \
                  cpu->cpuid, __FILE__, __LINE__, __func__, __lock->name, return_address(0)); \
        barrier();                                                                         \
        while (atomic_xchg(&__lock->lock, 1) == 1)                                         \
        {                                                                                  \
            popcli();                                                                      \
            CPU_RELAX();                                                                   \
            pushcli();                                                                     \
        }                                                                                  \
        __lock->cpu = cpu;                                                                 \
    }

/**
 * @brief release a spinlock
 *
 * @param __lock
 */
#define spin_unlock(__lock)                                                                \
    {                                                                                      \
        assert(__lock, "no lock");                                                         \
        if (!spin_holding(__lock))                                                         \
            panic("cpu%d: %s:%d: in %s() : not holding '%s' return [\e[0;013m%p\e[0m]\n",  \
                  cpu->cpuid, __FILE__, __LINE__, __func__, __lock->name, return_address(0)); \
        barrier();                                                                         \
        __lock->cpu = NULL;                                                                \
        atomic_write(&__lock->lock, 0);                                                    \
        popcli();                                                                          \
    }

/**
 * @brief try to acquire a spinlock
 *
 * @param __lock
 * @return int '1' if acquisition was successful(lock wasn't held before) and '0' if acquisition failed(lock is already held)
 */
static inline int spin_trylock(spinlock_t *__lock)
{
    assert(__lock, "no spinlock");
    pushcli();
    barrier();
    long held = !atomic_xchg(&__lock->lock, 1);
    if (held)
        __lock->cpu = cpu;
    else
        popcli();
    return held; // not the one's who locked it?
}

#define spin_assert_lock(lk) {if (!spin_holding(lk)) panic("%s:%d: cpu%d: caller must hold %s, \e[0;012mreturn [\e[0;015m%p\e[0m]\e[0m\n", __FILE__, __LINE__, cpu->cpuid, lk->name, return_address(0));}