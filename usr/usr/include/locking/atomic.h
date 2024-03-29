#ifndef LOCKS_ATOMIC_H
#define LOCKS_ATOMIC_H 1

#include <stdint.h>
#include <stdbool.h>

typedef volatile uintptr_t atomic_t;

static inline uintptr_t atomic_read(atomic_t *ptr)
{
    atomic_t ret;
    __atomic_load(ptr, &ret, __ATOMIC_SEQ_CST);
    return ret;
}

static inline void atomic_write(atomic_t *ptr, atomic_t val)
{
    __atomic_store(ptr, &val, __ATOMIC_SEQ_CST);
}

static inline void atomic_clear(atomic_t *ptr)
{
    atomic_write(ptr, 0);
}

static inline uintptr_t atomic_add(atomic_t *ptr, atomic_t val)
{
    return __atomic_fetch_add(ptr, val, __ATOMIC_SEQ_CST); 
}

static inline uintptr_t atomic_sub(atomic_t *ptr, atomic_t val)
{
    return __atomic_fetch_sub(ptr, val, __ATOMIC_SEQ_CST);
}

static inline uintptr_t atomic_incr(atomic_t *ptr)
{
    return atomic_add(ptr, 1);
}

static inline uintptr_t atomic_decr(atomic_t *ptr)
{
    return atomic_sub(ptr, 1);
}

static inline uintptr_t atomic_xchg(atomic_t *ptr, atomic_t val)
{
    atomic_t ret;
    __atomic_exchange(ptr, &val, &ret, __ATOMIC_SEQ_CST);
    return ret;
}

static inline uintptr_t atomic_or(atomic_t *ptr, atomic_t val)
{
    return __atomic_fetch_or(ptr, val, __ATOMIC_SEQ_CST);
}

static inline uintptr_t atomic_and(atomic_t *ptr, atomic_t val)
{
    return __atomic_fetch_and(ptr, val, __ATOMIC_SEQ_CST);
}

static inline uintptr_t atomic_xor(atomic_t *ptr, atomic_t val)
{
    return __atomic_fetch_xor(ptr, val, __ATOMIC_SEQ_CST);
}

static inline uintptr_t atomic_nand(atomic_t *ptr, atomic_t val)
{
    return __atomic_fetch_nand(ptr, val, __ATOMIC_SEQ_CST);
}

#endif // LOCKS_ATOMIC_H