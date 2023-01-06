#include <printk.h>
#include <sys/proc.h>
#include <mm/kalloc.h>
#include <bits/errno.h>
#include <lib/string.h>
#include <sys/thread.h>
#include <lime/string.h>
#include <locks/spinlock.h>
#include <arch/sys/thread.h>
#include <arch/sys/uthread.h>

void tgroup_free(tgroup_t *tgrp)
{
    thread_t *thread = NULL;
    assert(tgrp, "no tgrp pointer");
    if (tgrp->queue)
    {
        queue_lock(tgrp->queue);
        while ((thread = thread_dequeue(tgrp->queue)))
        {
            printk("trashing thread(%d)\n", thread->t_tid);
            thread_free(thread);
        }
        queue_free(tgrp->queue);
    }
    kfree(tgrp);
}

int tgroup_new(tid_t tgid, tgroup_t **ref)
{
    int err =0;
    queue_t *queue = NULL;
    tgroup_t *tgrp = NULL;

    assert(ref, "no tgrp poiter reference");
    if (!(tgrp = kmalloc(sizeof *tgrp)))
        return -ENOMEM;

    char *name = strcat_num("tgrp", tgid, 10);
    assert(name, "no memory to alloc tgrp name");

    if ((err = queue_new(name, &queue)))
        goto error;

    memset(tgrp, 0, sizeof *tgrp);

    tgrp->gid = tgid;
    tgrp->queue = queue;

    *ref = tgrp;
    if (name)
        kfree(name);
    return 0;

error:
    if (tgrp)
        kfree(tgrp);
    if (name)
        kfree(name);
    if (queue)
        queue_free(queue);
    printk("failed to create task group, error: %d\n", err);
    return err;
}

static atomic_t tids = 1;

static tid_t thread_tid_alloc(void)
{
    return (tid_t) atomic_incr(&tids);
}

int thread_new(thread_t **tref)
{
    int err = 0;
    tid_t tid = 0;
    char *name = NULL;
    queue_t *queues = NULL;
    spinlock_t *lock = NULL;
    thread_t *thread = NULL;
    cond_t *wait_cond = NULL;
    x86_thread_t *tarch = NULL;

    assert(tref, "no thread pointer reference");
    tid = thread_tid_alloc();
    name = strcat_num("thread", tid, 10);
    assert(name, "failed to alloc name for thread");

    if ((err = queue_new(name, &queues)))
        goto error;

    queue_lock(queues);

    if ((err = spinlock_init(NULL, name, &lock)))
        goto error;

    if ((err = cond_init(NULL, name, &wait_cond)))
        goto error;

    if ((err = arch_thread_alloc(&tarch)))
        goto error;

    if (!(thread = kmalloc(sizeof *thread)))
    {
        err = -ENOMEM;
        goto error;
    }

    memset(thread, 0, sizeof *thread);
    thread->t_tid = tid;
    thread->t_queues = queues;
    thread->t_lock = lock;
    thread->t_wait = wait_cond;
    thread->t_tarch = tarch;
    thread_lock(thread); // start out as locked thread
    sched_set_priority(thread, SCHED_LOWEST_PRIORITY);
    queue_unlock(queues);
    kfree(name);

    *tref = thread;
    return 0;
    
error:
    if (name)
        kfree(name);
    if (queues)
        queue_free(queues);
    if (lock)
        spinlock_free(lock);
    if (thread)
        kfree(thread);
    if (wait_cond)
        cond_free(wait_cond);
    if (tarch)
        arch_thread_free(tarch);
    printk("failed to create thread, error: %d\n", err);
    return err;
}

void thread_free(thread_t *thread)
{
    queue_t *q = NULL;
    thread_assert(thread);
    assert((thread->t_state == T_ZOMBIE) || (thread->t_state == T_EMBRYO), "freeing a non zombie thread");

    if (thread->t_wait)
        cond_free(thread->t_wait);
    if (thread->t_lock)
        spinlock_free(thread->t_lock);
    if (thread->t_queues)
    {
        queue_lock(thread->t_queues);
        while ((q = dequeue(thread->t_queues)))
        {
            queue_lock(q);
            queue_remove(q, thread);
            queue_unlock(q);
        }
        queue_free(thread->t_queues);
    }
    if (thread->t_tarch)
        arch_thread_free(thread->t_tarch);
    kfree(thread);
}

void thread_dump(thread_t *);

int thread_enqueue(queue_t *queue, thread_t *thread, queue_node_t **rnode)
{
    int err =0;
    queue_node_t *node = NULL;
    assert(queue, "no queue");
    assert(thread, "no thread");
    
    queue_lock(queue);
    queue_lock(thread->t_queues);
    if (!(node = enqueue(queue, (void *)thread)))
    {
        err = -ENOMEM;
        queue_unlock(thread->t_queues);
        queue_unlock(queue);
        goto error;
    }

    if (!enqueue(thread->t_queues, (void *)queue))
    {
        err = -ENOMEM;
        node = NULL;
        queue_remove(queue, (void *)thread);
        queue_unlock(thread->t_queues);
        queue_unlock(queue);
        goto error;
    }
    queue_unlock(thread->t_queues);
    queue_unlock(queue);

    if (rnode) *rnode = node;

    return 0;
error:
    printk("fialed to enqueue thread, error: %d\n", err);
    return err;
}

thread_t *thread_dequeue(queue_t *queue)
{
    thread_t *thread = NULL;
    assert(queue, "no queue");
    queue_assert_lock(queue);

    if (!(thread = dequeue(queue)))
        return NULL;
    thread_lock(thread);
    queue_lock(thread->t_queues);
    if (queue_remove(thread->t_queues, (void *)queue))
        panic("queue: \'%s\' not on \'%s\'\n", queue->name, thread->t_queues->name);
    queue_unlock(thread->t_queues);
    return thread;
}

int thread_execve(proc_t *proc, thread_t *thread, void *(*entry)(void *), const char *argp[], const char *envp[])
{
    int err = 0;
    vmr_t *stack = NULL;
    thread_assert(thread);
    shm_assert_lock(proc->mmap);

    if ((err = shm_alloc_stack(proc->mmap, &stack)))
        goto error;

    thread->t_tarch->ustack = stack;
    if ((err = arch_uthread_init(thread->t_tarch, entry, argp, envp)))
        goto error;

    return 0;
error:
    if (stack)
    {
        vmr_lock(stack);
        shm_remove(proc->mmap, stack);
        vmr_unlock(stack);
    }
    return err;
}

int thread_fork(thread_t *dst, thread_t *src)
{
    int err = 0;
    vmr_t *vmr = NULL;

    thread_assert(src);
    thread_assert(dst);
    thread_assert_lock(src);
    thread_assert_lock(dst);
    assert(src->t_tarch, "source has no tarch");
    assert(dst->t_tarch, "destination has no tarch");

    dst->t_sched_attr = src->t_sched_attr;
    atomic_write(&dst->t_sched_attr.age, 0);

    if (!(vmr = shm_lookup(dst->mmap, ((trapframe_t *)src->t_tarch->tf)->esp)))
    {
        err = -EADDRNOTAVAIL;
        goto error;
    }

    dst->t_tarch->ustack = vmr;

    if ((err = arch_thread_fork(dst->t_tarch, src->t_tarch)))
        goto error;

    return 0;
error:
    klog(KLOG_FAIL, "failed to fork thread, error=%d\n", err);
    return err;
}