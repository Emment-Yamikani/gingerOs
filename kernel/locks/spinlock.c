#include <locks/barrier.h>
#include <locks/spinlock.h>
#include <mm/kalloc.h>
#include <lime/string.h>
#include <lib/string.h>
#include <lime/assert.h>
#include <bits/errno.h>

void spinlock_free(spinlock_t *__lock)
{
    if (!__lock)
        return;
    if (__lock->name && (__lock->flags & 2))
        kfree(__lock->name);
    if (!(__lock->flags & 1))
        return;
    kfree(__lock);
}

int spinlock_init(const spinlock_t *__lock, const char *__name, spinlock_t **__ref)
{
    char *name = NULL;
    spinlock_t *lk = NULL;
    if ((!__lock && !__ref) || !__name) return -EINVAL;

    if (__lock) lk = (spinlock_t *)__lock;
    else if (!(lk = (spinlock_t *)kmalloc(sizeof(*lk))))
        return -ENOMEM;

    if (!(name = combine_strings(__name, "-spinlock")))
    {
        if (!__lock) kfree(lk);
        return -ENOMEM;
    }

    memset(lk, 0, sizeof *lk);
    
    if (!__lock) lk->flags = 1;
    lk->flags |= 2; 
    lk->name = name;
    if (__ref) *__ref = lk;
    return 0;
}