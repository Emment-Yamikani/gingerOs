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

tid_t thread_self(void)
{
    current_assert();
    return current->t_tid;
}

void thread_exit(uintptr_t exit_code)
{
    current_assert();
    arch_thread_exit(exit_code);
}

int thread_wake(thread_t *thread)
{
    cond_t *cond = NULL;
    mutex_t *mutex = NULL;

    thread_assert(thread);
    thread_assert_lock(thread);
    assert(thread->sleep.queue, "sleeping thread has no queue");
    
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
    thread_assert_lock(thread);
    atomic_write(&thread->t_killed, thread_self());
    return 0;
}

int thread_kill(tid_t tid)
{
    int err = 0;
    thread_t *thread = NULL;
    if (tid < 0)
        return -EINVAL;

    current_assert();
    if (atomic_read(&current->t_killed))
        return -ERFKILL;
    assert(current->t_group, "no t_group");
    if ((err = thread_get(current->t_group, tid, &thread)))
        return err;

    thread_assert_lock(thread);
    if (thread == current)
    {
        thread_unlock(thread);
        thread_exit(0);
    }

    if ((err = thread_kill_n(thread)))
    {
        thread_unlock(thread);
        return err;
    }
    thread_unlock(thread);
    return 0;
}

int thread_wait(thread_t *thread, void **retval)
{
    thread_assert_lock(thread);
    for (;;)
    {
        if (atomic_read(&current->t_killed))
        {
            thread_unlock(thread);
            return -ERFKILL;
        }

        if (thread->t_state == T_ZOMBIE)
        {
            if (retval)
                *retval = (void *)thread->t_exit;
            thread_free(thread);
            break;
        }

        thread_unlock(thread);
        cond_wait(thread->t_wait);
        thread_lock(thread);
    }
    return 0;
}

int thread_kill_all(void)
{
    tgroup_t *tgrp = NULL;
    thread_t *thread = NULL;

    current_assert();
    tgrp = current->t_group;
    if (tgrp == NULL)
        return -ENOSYS;

    if (atomic_read(&current->t_killed))
        return -ERFKILL;

    queue_lock(tgrp->queue);
    forlinked(node, tgrp->queue->head, node->next)
    {
        thread = node->data;
        if (thread == current)
            continue;
        thread_lock(thread);
        thread_kill_n(thread);
        queue_unlock(tgrp->queue);
        thread_wait(thread, NULL);
        queue_lock(tgrp->queue);
    }
    queue_unlock(tgrp->queue);

    while (atomic_read(&tgrp->nthreads) > 1)
        ;
    return 0;
}

int thread_join(tid_t tid, void **retval)
{
    int err = 0;
    thread_t *thread = NULL;
    if (tid < 0)
        return -EINVAL;

    current_assert();
    if (atomic_read(&current->t_killed))
        return -ERFKILL;
    assert(current->t_group, "no t_group");
    if ((err = thread_get(current->t_group, tid, &thread)))
        return err;

    thread_assert_lock(thread);
    if (thread == current)
    {
        thread_unlock(thread);
        return -EDEADLOCK;
    }

    for (;;)
    {
        if (atomic_read(&current->t_killed))
        {
            thread_unlock(thread);
            return -ERFKILL;
        }

        if (thread->t_state == T_ZOMBIE)
        {
            if (retval)
                *retval = (void *)thread->t_exit;
            thread_free(thread);
            break;
        }

        thread_unlock(thread);
        cond_wait(thread->t_wait);
        thread_lock(thread);
    }

    return 0;
}

void thread_yield(void)
{
    sched_yield();
}

int thread_create(tid_t *tid, void *(*entry)(void *), void *arg)
{
    int err = 0;
    vmr_t *stack = NULL;
    thread_t *thread = NULL;

    current_assert();
    if (proc == NULL)
        goto kernel_thread;

    if ((err = thread_new(&thread)))
        goto error;

    thread->mmap = current->mmap;
    thread->owner = current->owner;
    thread->t_gid = current->t_gid;
    thread->t_group = current->t_group;
    thread->t_file_table = current->t_file_table;

    sched_set_priority(thread, SCHED_LOWEST_PRIORITY);

    shm_lock(current->mmap);
    if ((err = shm_alloc_stack(proc->mmap, &stack)))
        goto error;

    thread->t_tarch->ustack = stack;

    if ((err = arch_uthread_create(thread->t_tarch, entry, arg)))
    {
        shm_unlock(current->mmap);
        goto error;
    }

    if ((err = thread_enqueue(current->t_group->queue, thread, NULL)))
    {
        shm_unlock(current->mmap);
        goto error;
    }

    goto done;

kernel_thread:
    if ((err = kthread_create(entry, arg, tid, &thread)))
        goto error;

done:
    atomic_incr(&current->t_group->nthreads);
    *tid = thread->t_tid;
    shm_unlock(current->mmap);

    if ((err = sched_park(thread)))
        goto error;

    thread_unlock(thread);
    return 0;
error:
    klog(KLOG_FAIL, "failed to create thread");
    return err;
}