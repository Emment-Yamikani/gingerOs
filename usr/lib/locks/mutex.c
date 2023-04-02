#include <ginger.h>
#include <locking/mutex.h>

int mutex_init(mutex_t *mtx)
{
    if (mtx == NULL)
        return -EINVAL;
    memset(mtx, 0, sizeof *mtx);
    return spinlock_init(&mtx->guard);
}

void mutex_lock(mutex_t *mtx)
{
    int err = 0;
    if (mtx == NULL)
        panic("Thread(%d) is trying to hold null mutex\n", thread_self());
    
    spin_lock(&mtx->guard);
    if (atomic_read(&mtx->lock) == 0)
        atomic_write(&mtx->lock, 1);
    else
    {
        if ((err = glist_add(&mtx->list, (void *)thread_self())))
            panic("%s: %s:%d: thread(%d), error(%d)\n", __FILE__, __func__, __LINE__, thread_self(), err);
        setpark();
        spin_unlock(&mtx->guard);
        if ((err = park()))
            panic("%s: %s:%d: thread(%d), error(%d)\n", __FILE__, __func__, __LINE__, thread_self(), err);
        spin_lock(&mtx->guard);
    }

    mtx->threadID = thread_self();
    spin_unlock(&mtx->guard);
}

void mutex_unlock(mutex_t *mtx)
{
    tid_t threadID = 0;
    if (mtx == NULL)
        panic("Thread(%d) is trying to hold null mutex\n", thread_self());
    
    spin_lock(&mtx->guard);

    if ((threadID = (tid_t)glist_get(&mtx->list)))
        unpark(threadID);
    else
        atomic_write(&mtx->lock, 0);

    spin_unlock(&mtx->guard);
}

int mutex_trylock(mutex_t *mtx)
{
    int held = 0;
    if (mtx == NULL)
        return -EINVAL;
    spin_lock(&mtx->guard);
    if ((held = !atomic_xchg(&mtx->lock, 1)))
        mtx->threadID = thread_self();
    spin_unlock(&mtx->guard);
    return !held;
}

int mutex_holding(mutex_t *mtx)
{
    return ((mtx->threadID == thread_self()) && (atomic_read(&mtx->lock) != 0));
}