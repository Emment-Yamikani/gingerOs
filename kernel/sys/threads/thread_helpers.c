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
    return thread_wake_n(thread);
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
    if ((__thread_zombie(thread)) ||
        (__thread_terminated(thread)) ||
        __thread_killed(thread))
        return 0;
    __thread_setflags(thread, THREAD_KILLED);
    atomic_write(&thread->t_killer, thread_self());
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
    int err = 0;
    thread_assert_lock(thread);

    loop()
    {
        if (__thread_killed(current))
            return -EINTR;

        if ((thread->t_state == T_ZOMBIE) || (thread->t_state == T_TERMINATED))
        {
            if (retval)
                *retval = (void *)thread->t_exit;
            if (reap)
                thread_free(thread);
            break;
        }

        thread_unlock(thread);
        if ((err = cond_wait(thread->t_wait))) {
            thread_lock(thread);
            return err;
        }
        thread_lock(thread);
    }

    return 0;
}

int thread_kill_all(void)
{
    current_assert();
    return tgroup_kill_thread(current->t_group, -1);
}

int thread_wake_n(thread_t *thread)
{
    int err = 0, held = 0;
    thread_assert_lock(thread);
    
    if (__thread_testflags(thread, THREAD_SETPARK))
        __thread_setflags(thread, THREAD_SETWAKEUP);

    if (!__thread_isleep(thread) || __thread_zombie(thread) || __thread_terminated(thread))
        return 0;
    
    if (thread->sleep.queue == NULL)
        panic("%s:%d: No wait queue\n", __FILE__, __LINE__, thread->t_tid);
    
    if (thread->sleep.guard){
        if (!spin_holding(thread->sleep.guard)){
            spin_lock(thread->sleep.guard);
            held = 1;
        }
    }
    
    err = thread_remove_queue(thread, thread->sleep.queue);

    if (thread->sleep.guard){
        if (spin_holding(thread->sleep.guard) && held){
            spin_unlock(thread->sleep.guard);
            held = 0;
        }
    }

    if (err) return err;

    if (__thread_zombie(thread) || __thread_terminated(thread))
        return 0;

    if ((err = __thread_enter_state(thread, T_READY)))
        return err;

    return sched_park(thread);
}

int queue_get_thread(queue_t *queue, tid_t tid, thread_t **pthread)
{
    thread_t *thread = NULL;
    queue_node_t *next = NULL;

    if (!queue || !pthread)
        return -EINVAL;
    
    if (tid < 0)
        return -EINVAL;

    if (tid == thread_self())
    {
        *pthread = current;
        return 0;
    }

    queue_assert_lock(queue);
    forlinked(node, queue->head, next)
    {
        next = node->next;
        thread = node->data;

        thread_lock(thread);
        if (thread->t_tid == tid)
        {
            *pthread = thread;
            return 0;
        }
        thread_unlock(thread);
    }

    return -ESRCH;
}