#include <bits/errno.h>
#include <lib/string.h>
#include <locks/event.h>
#include <mm/kalloc.h>
#include <printk.h>
#include <sys/thread.h>
#include <sys/sched.h>

void event_free(event_t *ev)
{
    if (!ev)
        return;
    if (ev->queue)
    {
        sched_wakeall(ev->queue);
        queue_lock(ev->queue);
        queue_free(ev->queue);
        ev->queue = NULL;
    }
    if (ev->guard)
        spinlock_free(ev->guard);
    ev->guard = NULL;
    kfree(ev);
}

int event_new(const char *__name, event_t **_ev)
{
    int err = 0;
    event_t *ev = NULL;
    queue_t *q = NULL;
    spinlock_t *lk = NULL;

    if (!__name || !ev)
        return -EINVAL;
    
    if ((err = spinlock_init(NULL, __name, &lk)))
        goto error;
    
    if ((err = queue_new(__name, &q)))
        goto error;

    if ((ev = kmalloc(sizeof *ev)))
    {
        err = -ENOMEM;
        goto error;
    }

    memset(ev, 0, sizeof *ev);
    
    ev->guard = lk;
    ev->queue = q;
    
    *_ev = ev;
    return 0;
error:
    if (ev)
        kfree(ev);
    if (lk)
        spinlock_free(lk);
    if (q)
    {
        queue_lock(q);
        queue_free(q);
    }
    return err;
}

int event_wait(event_t *ev)
{
    if (!ev)
        return -EINVAL;
    
    spin_lock(ev->guard);
    if (atomic_read(&ev->event) == 0) // has event occured yet?
    {
        // event hasn't occuered yet.
        current_lock();
        // so sleep wating for the condition to be true.
        if (sched_sleep(ev->queue, ev->guard))
        {
            // we were woken up but not for this event.
            current_unlock();
            spin_unlock(ev->guard);
            return -EINTR;
        }

        // we were woken up becaused the event has actually occured.
        current_unlock();
    }
    spin_unlock(ev->guard);
    return 0;
}

void event_signal(event_t *ev)
{
    if (!ev)
        return;
    
    spin_lock(ev->guard);
    atomic_write(&ev->event, 1); // set the condition
    if (sched_wake1(ev->queue)) // have we woken a thread ?
        atomic_clear(&ev->event); // if so, clear the condition, as it no long holds.
    spin_unlock(ev->guard);
}

int event_broadcast(event_t *ev)
{
    if (!ev)
        return -EINVAL;
    spin_lock(ev->guard);
    if (sched_wakeall(ev->queue) == 0)
        atomic_write(&ev->event, 1);
    atomic_clear(&ev->event);
    spin_unlock(ev->guard);
    return 0;
}