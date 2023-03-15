#include <printk.h>
#include <mm/kalloc.h>
#include <bits/errno.h>
#include <lib/string.h>
#include <sys/thread.h>
#include <lime/string.h>
#include <locks/spinlock.h>
#include <arch/sys/thread.h>
#include <arch/sys/uthread.h>
#include <sys/kthread.h>
#include <sys/proc.h>
#include <arch/i386/cpu.h>
#include <arch/sys/proc.h>
#include <locks/mutex.h>

int thread_wake(thread_t *thread)
{
    cond_t *cond = NULL;
    mutex_t *mutex = NULL;

    thread_assert(thread);
    thread_assert_lock(thread);

    if (thread->sleep.queue == NULL)
        return 0;
    
    switch (thread->sleep.type)
    {
    case CONDITION:
        cond = thread->sleep.data;
        cond_remove(cond, thread);
        break;
    case MUTEX:
        mutex =  thread->sleep.data;
        mutex_remove(mutex, thread);
        break;
    default:
        panic("sleeping thread has no sleep struct type\n");
    }

    if ((thread->t_state == T_ZOMBIE) || (thread->t_state == T_TERMINATED))
        return 0;
    // park thread ready for running
    thread->t_state = T_READY;
    return sched_park(thread);
}

int thread_get(tgroup_t *tgrp, tid_t tid, thread_t **tref)
{
    thread_t *thread = NULL;
    assert(tgrp, "no tgrp");
    assert(tref, "no thread reference");
    assert(tgrp->queue, "no tgrp->queue");

    queue_lock(tgrp->queue);
    forlinked(node, tgrp->queue->head, node->next)
    {
        thread = node->data;
        if (thread->t_tid == tid)
        {
            *tref = thread;
            thread_lock(thread)
                queue_unlock(tgrp->queue);
            return 0;
        }
    }
    queue_unlock(tgrp->queue);

    return -ENOENT;
}

int thread_kill_n(thread_t *thread)
{
    thread_assert(thread);
    if (thread == current)
        return 0;
    thread_assert_lock(thread);
    if ((thread->t_state == T_ZOMBIE) ||
        (thread->t_state == T_TERMINATED) ||
        atomic_read(&thread->t_killed))
        return 0;
    atomic_write(&thread->t_killed, thread_self());
    return 0;
}

int thread_kill(tid_t tid)
{
    tgroup_t *tgroup = NULL;

    if (tid < 0)
        return -EINVAL;

    current_lock();
    tgroup = current->t_group;
    current_unlock();

    return tgroup_kill_thread(tgroup, tid);
}

int thread_wait(thread_t *thread, int reap, void **retval)
{
    thread_assert_lock(thread);
    for (;;)
    {
        if (atomic_read(&current->t_killed))
            return -ERFKILL;

        if ((thread->t_state == T_ZOMBIE) || (thread->t_state == T_TERMINATED))
        {
            if (retval)
                *retval = (void *)thread->t_exit;
            if (reap)
                thread_free(thread);
            break;
        }

        thread_unlock(thread);
        cond_wait(thread->t_wait);
        thread_lock(thread);
    }
    return 0;
}

int thread_ishandling_signal(thread_t *thread)
{
    return (atomic_read(&thread->t_flags) & THREAD_HANDLING_SIGNAL);
}

int thread_kill_all(void)
{
    tgroup_t *tgrp = NULL;
    current_assert();
    tgrp = current->t_group;
    return tgroup_kill_thread(tgrp, -1);
}