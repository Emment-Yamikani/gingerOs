#include <ginger.h>
#include <locking/cond.h>

int cv_init(cv_t *cv)
{
    if (!cv) return -EINVAL;
    memset(cv, 0, sizeof *cv);
    return mutex_init(&cv->guard);
}

int cv_wait(cv_t *cv)
{
    int err = 0;
    if (!cv) return -EINVAL;
    mutex_lock(&cv->guard);
    if (atomic_read(&cv->event) == 0)
    {
        if ((err = glist_add(&cv->queue, (void *)thread_self())))
        {
            mutex_unlock(&cv->guard);
            return err;
        }
        setpark();
        mutex_unlock(&cv->guard);
        if ((err = park())) return err;
        mutex_lock(&cv->guard);
    }
    atomic_write(&cv->event, 0);
    mutex_unlock(&cv->guard);
    return 0;
}

void cv_signal(cv_t *cv)
{
    tid_t threadID = 0;
    if (!cv) return;
    mutex_lock(&cv->guard);
wakeup:
    if ((threadID = (tid_t)glist_get(&cv->queue)))
    {
        if (unpark(threadID)) goto wakeup;
        atomic_write(&cv->event, 0);
    }
    else atomic_write(&cv->event, 1);
    mutex_unlock(&cv->guard);
}

void cv_broadcast(cv_t *cv)
{
    int count = 0;
    tid_t threadID = 0;
    if (!cv) return;
    mutex_lock(&cv->guard);
wakeup:
    if ((threadID = (tid_t)glist_get(&cv->queue)))
    {
        atomic_write(&cv->event, 0);
        if (unpark(threadID) == 0) count++;
        goto wakeup;
    }
    else if (count == 0) atomic_write(&cv->event, 1);
    mutex_unlock(&cv->guard);
}