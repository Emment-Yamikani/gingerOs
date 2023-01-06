#include <arch/i386/cpu.h>
#include <sys/sched.h>
#include <sys/thread.h>
#include <printk.h>
#include <bits/errno.h>
#include <lime/assert.h>

__unused static queue_t *embryo_queue = QUEUE_NEW("embryo-queue");
__unused static queue_t *zombie_queue = QUEUE_NEW("zombie-queue");

int sched_park(thread_t *thread)
{
    int err = 0;
    long priority = 0;
    long affinity = 0;
    cpu_t *core = NULL;
    level_t *level = NULL;

    assert(thread, "no thread");
    thread_assert_lock(thread);

    if (thread->t_state == T_EMBRYO)
        return thread_enqueue(embryo_queue, thread, NULL);

    core = thread->t_sched_attr.core;
    affinity = atomic_read(&thread->t_sched_attr.affinity);
    priority = atomic_read(&thread->t_sched_attr.t_priority);

    if (core == NULL)
        core = thread->t_sched_attr.core = cpu;

    switch (affinity)
    {
    case SCHED_SOFT_AFFINITY:
        level = &core->ready_queue->level[SCHED_LEVEL(priority)];
        break;
    case SCHED_HARD_AFFINITY:
        level = &core->ready_queue->level[SCHED_LEVEL(priority)];
        break;
    default:
        panic("Invalid affinity attribute\n");
    }

    atomic_write(&thread->t_sched_attr.t_timeslice, level->quatum);
    if ((err = thread_enqueue(level->queue, thread, NULL)))
        goto error;
    return 0;
error:
    printk("\e[0;4mfailed to put on ready queue\e[0m\n");
    return err;
}

thread_t *sched_next(void)
{
    level_t *level = NULL;
    thread_t *thread = NULL;

    queue_lock(embryo_queue);
    thread = thread_dequeue(embryo_queue);
    queue_unlock(embryo_queue);

    if (thread == NULL)
        goto self;

    thread_assert_lock(thread);
    thread->t_state = T_READY;
    assert(!sched_park(thread), "failed to park thread");
    thread_unlock(thread); // release thread

self:
    thread = NULL;
    pushcli();
    for (int i = 0; i < NLEVELS; ++i)
    {
        level = &ready_queue->level[i];
        queue_lock(level->queue);
        if (!(thread = thread_dequeue(level->queue)))
        {
            queue_unlock(level->queue);
            continue;
        }
        thread->t_state = T_RUNNING;
        atomic_write(&thread->t_sched_attr.t_timeslice, level->quatum);
        queue_unlock(level->queue);
        break;
    }
    popcli();
    return thread;
}

int sched_sleep(queue_t *sleep_queue, spinlock_t *lock)
{
    int err = 0;
    current_assert();
    queue_assert(sleep_queue);

    current_lock();
    if ((err = thread_enqueue(sleep_queue, current, &current->t_sleep_node)))
    {
        current_unlock();
        return err;
    }

    current->t_state = T_ISLEEP;
    current->t_sleep_queue = sleep_queue;
    if (lock)
        spin_unlock(lock);

    sched();

    if (lock)
        spin_lock(lock);
    current_unlock();

    if (atomic_read(&current->t_killed))
        return -EINTR;

    return 0;
}

void sched_wake1(queue_t *sleep_queue)
{
    thread_t *thread = NULL;
    queue_assert(sleep_queue);

    queue_lock(sleep_queue);
    thread = thread_dequeue(sleep_queue);
    queue_unlock(sleep_queue);

    if (thread == NULL)
        return;

    thread_assert_lock(thread);
    thread->t_state = T_READY;
    thread->t_sleep_node = NULL;
    thread->t_sleep_queue = NULL;
    sched_park(thread);
    thread_unlock(thread);
}

int sched_wakeall(queue_t *sleep_queue)
{
    int count = 0;
    thread_t *thread = NULL;
    queue_assert(sleep_queue);

    queue_lock(sleep_queue);
    count = queue_count(sleep_queue);

    forlinked(node, sleep_queue->head, node->next)
    {
        thread = node->data;
        thread_lock(thread);
        assert(!thread_wake(thread), "failed to park thread");
        thread_unlock(thread);
    }

    queue_unlock(sleep_queue);

    return count;
}

int sched_zombie(thread_t *thread)
{
    int err = 0;
    thread_assert(thread);
    thread_assert_lock(thread);
    if ((err = thread_enqueue(zombie_queue, thread, NULL)))
        return err;
    atomic_decr(&thread->t_group->nthreads);
    cond_broadcast(thread->t_wait);
    return 0;
}