#include <printk.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/thread.h>
#include <bits/errno.h>
#include <lime/assert.h>
#include <sys/kthread.h>
#include <locks/atomic.h>
#include <arch/i386/cpu.h>
#include <arch/sys/kthread.h>
#include <arch/sys/uthread.h>

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

int thread_join(tid_t tid, void **retval)
{
    int err = 0;
    thread_t *thread = NULL;
    if (tid < 0)
        return -EINVAL;

    printk("%s(%d, %p)\n", __func__, thread_self(), tid, retval);

    current_assert();

    if (__thread_killed(current))
        return -EINTR;

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
        if (__thread_killed(current))
            return -EINTR;

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

    mmap_lock(current->mmap);
    if ((err = mmap_alloc_stack(proc->mmap, USTACKSIZE, &stack)))
        goto error;

    thread->t_tarch->ustack = stack;

    if ((err = arch_uthread_create(thread->t_tarch, entry, arg)))
    {
        mmap_unlock(current->mmap);
        goto error;
    }

    if ((err = thread_enqueue(current->t_group->queue, thread, NULL)))
    {
        mmap_unlock(current->mmap);
        goto error;
    }

    goto done;

kernel_thread:
    if ((err = kthread_create(entry, arg, tid, &thread)))
        goto error;

done:
    atomic_incr(&current->t_group->nthreads);
    *tid = thread->t_tid;
    mmap_unlock(current->mmap);

    if ((err = sched_park(thread)))
        goto error;

    thread_unlock(thread);
    return 0;
error:
    klog(KLOG_FAIL, "failed to create thread");
    return err;
}

int thread_cancel(tid_t tid)
{
    (void)tid;

    return -ENOSYS;
}