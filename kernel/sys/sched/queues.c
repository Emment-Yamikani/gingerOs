#include <arch/i386/cpu.h>
#include <sys/sched.h>
#include <sys/thread.h>
#include <printk.h>
#include <bits/errno.h>
#include <lime/assert.h>

static queue_t *embryo_queue = QUEUE_NEW("embryo-queue");
static queue_t *zombie_queue = QUEUE_NEW("zombie-queue");

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
    current_assert_lock();

    if ((err = thread_enqueue(sleep_queue, current, &current->sleep.node)))
        return err;

    current->t_state = T_ISLEEP;
    current->sleep.queue = sleep_queue;

    //printk("sleep_node: %p, sleep_node->next: %p\n", current->t_sleep_node, current->t_sleep_node->next);
    if (lock)
        spin_unlock(lock);

    sched();

    if (lock)
        spin_lock(lock);

    current->sleep.queue = NULL;
    current->sleep.node = NULL;

    if (atomic_read(&current->t_killed))
        return -EINTR;

    return 0;
}

int sched_wake1(queue_t *sleep_queue)
{
    thread_t *thread = NULL;
    queue_assert(sleep_queue);

    queue_lock(sleep_queue);
    thread = thread_dequeue(sleep_queue);
    queue_unlock(sleep_queue);

    if (thread == NULL)
        return 0;

    thread_assert_lock(thread);
    thread->t_state = T_READY;
    
    sched_park(thread);
    thread_unlock(thread);
    return 1;
}

int sched_wakeall(queue_t *sleep_queue)
{
    int count = 0;
    thread_t *thread = NULL;
    queue_node_t *next = NULL;
    queue_assert(sleep_queue);

    queue_lock(sleep_queue);
    count = queue_count(sleep_queue);

    forlinked(node, sleep_queue->head, next)
    {
        thread = node->data;
        next = node->next;
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

int xched_sleep(queue_t *sleep_queue, spinlock_t *lock)
{
    int err = 0;
    current_assert();
    queue_assert(sleep_queue);
    current_assert_lock();

    if ((err = thread_enqueue(sleep_queue, current, &current->sleep.node)))
        return err;

    current->sleep.guard = lock;
    __thread_enter_state(current, T_ISLEEP);
    current->sleep.queue = sleep_queue;

    // printk("sleep_node: %p, sleep_node->next: %p\n", current->t_sleep_node, current->t_sleep_node->next);
    if (lock)
        spin_unlock(lock);

    sched();

    if (lock)
        spin_lock(lock);

    current->sleep.queue = NULL;
    current->sleep.node = NULL;

    if (atomic_read(&current->t_killed))
        return -EINTR;

    return 0;
}

int xched_wake1(queue_t *sleep_queue)
{
    int woken = 0;
    thread_t *thread = NULL;
    queue_assert(sleep_queue);

    queue_lock(sleep_queue);
    thread = thread_dequeue(sleep_queue);
    queue_unlock(sleep_queue);

    if (thread == NULL)
        return 0;

    thread_assert_lock(thread);
    __thread_enter_state(thread, T_READY);

    sched_park(thread);
    thread_unlock(thread);
    return woken;
}

int xched_wakeall(queue_t *sleep_queue)
{
    int count = 0;
    thread_t *thread = NULL;
    queue_node_t *next = NULL;
    queue_assert(sleep_queue);

    queue_lock(sleep_queue);
    count = queue_count(sleep_queue);

    forlinked(node, sleep_queue->head, next)
    {
        next = node->next;
        thread = node->data;
        thread_lock(thread);
        assert(!thread_wake_n(thread), "failed to park thread");
        thread_unlock(thread);
    }

    queue_unlock(sleep_queue);
    return count;
}