#include <locks/mutex.h>
#include <mm/kalloc.h>
#include <printk.h>
#include <lib/string.h>
#include <lime/string.h>
#include <bits/errno.h>
#include <sys/thread.h>

void mutex_free(mutex_t *mutex)
{
    mutex_assert(mutex);
    if (mutex->guard)
        spinlock_free(mutex->guard);
    if (mutex->waiters)
    {
        queue_lock(mutex->waiters);
        queue_free(mutex->waiters);
    }
    kfree(mutex);
}

int mutex_init(mutex_t *mutex, mutex_t **mref)
{
    int err = 0;
    int alloc = !mutex;
    spinlock_t *lock = NULL;
    queue_t *waiters = NULL;

    assert((mutex || mref), "no pointer specified");
    if ((err = spinlock_init(NULL, "mutex", &lock)))
        goto error;
    if ((err = queue_new("mutex", &waiters)))
        goto error;

    if (alloc)
    {
        if (!(mutex = kmalloc(sizeof *mutex)))
        {
            err = -ENOMEM;
            goto error;
        }
    }

    memset(mutex, 0, sizeof *mutex);
    mutex->guard = lock;
    mutex->waiters = waiters;

    *mref = mutex;
    return 0;
error:
    if (waiters)
    {
        queue_lock(waiters);
        queue_free(waiters);
    }
    if (lock)
        spinlock_free(lock);
    klog(KLOG_FAIL, "failed to allocate mutex, error: %d\n", err);
    return err;
}

int mutex_lock(mutex_t *mutex)
{
    int retval = 0;
    mutex_assert(mutex);
    
    pushcli();
    
    spin_lock(mutex->guard);

    if (mutex_holding(mutex))
        panic("thread(%d) holding mutex return [\e[0;013m%p\e[0m]\n", thread_self(), return_address(0));

    if ((int)atomic_incr(&mutex->lock) > 0)
        retval = sched_sleep(mutex->waiters, mutex->guard);

    mutex->thread = current;
    spin_unlock(mutex->guard);

    return retval;
}

void mutex_unlock(mutex_t *mutex)
{
    mutex_assert(mutex);
    spin_lock(mutex->guard);
    mutex_assert_lock(mutex);
    mutex->thread = NULL;
    if ((int)atomic_decr(&mutex->lock) <= 0)
        panic("mutex->lock underflow: %d\n", atomic_read(&mutex->lock));
    sched_wake1(mutex->waiters);
    spin_unlock(mutex->guard);
    popcli();
}

int mutex_try_lock(mutex_t *mutex)
{
    mutex_assert(mutex);
    spin_lock(mutex->guard);
    pushcli();
    if (mutex_holding(mutex))
        panic("%s:%d: thread(%d) holding mutex\n", __FILE__, __LINE__, thread_self());
    if (atomic_xchg(&mutex->lock, 1) == 0) // we have acquired the mutex
    {
        mutex->thread = current;
        spin_unlock(mutex->guard);
        return 1; // success
    }
    popcli();
    spin_unlock(mutex->guard);
    return 0; // unsuccessful
}