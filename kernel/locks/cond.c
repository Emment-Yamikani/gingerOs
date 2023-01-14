#include <bits/errno.h>
#include <locks/cond.h>
#include <mm/kalloc.h>
#include <lib/string.h>
#include <lime/string.h>
#include <printk.h>
#include <sys/thread.h>
#include <sys/sched.h>
#include <lime/assert.h>

int cond_free(cond_t *c)
{
    if (!c)
        return -EINVAL;
    if (c->waiters)
    {
        queue_lock(c->waiters);
        queue_free(c->waiters);
    }
    if (c->name)
        kfree(c->name);
    kfree(c);
    return 0;
}

int cond_init(cond_t *c, char *name, cond_t **ref)
{
    int err = 0;
    int alloc = !c;
    spinlock_t *lk = NULL;
    queue_t *waiters = NULL;
    if ((!c && !ref) || !name)
        return -EINVAL;
    if (alloc)
    {
        if (!(c = __cast_to_type(c) kmalloc(sizeof *c)))
        {
            err = -ENOMEM;
            goto error;
        }
    }
    if (!(name = combine_strings(name, "-condition")))
    {
        err = -EINVAL;
        goto error;
    }
    if ((err = queue_new(name, &waiters)))
        goto error;

    if ((err = spinlock_init(NULL, name, &lk)))
        goto error;

    *c = (cond_t){0};

    c->guard = lk;
    c->name = name;
    c->waiters = waiters;

    if (ref)
        *ref = c;
    return 0;
error:
    if (c && alloc)
        kfree(c);
    if (name)
        kfree(name);
    printk("cond_init(): called @ 0x%p, error=%d\n", return_address(0), err);
    return err;
}

int cond_wait(cond_t *cond)
{
    int retval = 0;
    assert(cond, "no condition-variable");

    current_assert();
    spin_lock(cond->guard);
    if ((int)atomic_incr(&cond->count) >= 0)
    {
        current_lock();
        current->sleep.data = cond;
        current->sleep.type = CONDITION;
        
        retval = sched_sleep(cond->waiters, cond->guard);
        
        current->sleep.data = NULL;
        current->sleep.type = INVALID;
        current_unlock();
    }
    spin_unlock(cond->guard);
    return retval;
}

static void cond_wake1(cond_t *cond)
{
    sched_wake1(cond->waiters);
}

void cond_signal(cond_t *cond)
{
    assert(cond, "no condition-variable");
    spin_lock(cond->guard);
    cond_wake1(cond);
    atomic_decr(&cond->count);
    spin_unlock(cond->guard);
}

static void cond_wakeall(cond_t *cond)
{
    int waiters = sched_wakeall(cond->waiters);
    if (waiters == 0)
        atomic_write(&cond->count, -1);
    else
        atomic_write(&cond->count, 0);
}

void cond_broadcast(cond_t *cond)
{
    assert(cond, "no condition-variable");
    spin_lock(cond->guard);
    cond_wakeall(cond);
    spin_unlock(cond->guard);
}