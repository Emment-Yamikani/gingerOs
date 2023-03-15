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

static atomic_t tids = 1;

void tgroup_free(tgroup_t *tgroup)
{
    thread_t *thread = NULL;
    assert(tgroup, "no tgroup pointer");
    if (tgroup->queue)
    {
        queue_lock(tgroup->queue);
        while ((thread = thread_dequeue(tgroup->queue)))
        {
            printk("\e[07;04mtrashing thread(%d)\e[0m\n", thread->t_tid);
            thread_free(thread);
        }
        queue_free(tgroup->queue);
    }
    kfree(tgroup);
}

int tgroup_new(tid_t tgid, tgroup_t **ref)
{
    int err =0;
    queue_t *queue = NULL;
    tgroup_t *tgroup = NULL;

    assert(ref, "no tgroup poiter reference");
    if (!(tgroup = kmalloc(sizeof *tgroup)))
        return -ENOMEM;

    char *name = strcat_num("tgroup", tgid, 10);
    assert(name, "no memory to alloc tgroup name");

    if ((err = queue_new(name, &queue)))
        goto error;

    memset(tgroup, 0, sizeof *tgroup);

    tgroup->gid = tgid;
    tgroup->queue = queue;

    *ref = tgroup;
    if (name)
        kfree(name);
    return 0;

error:
    if (tgroup)
        kfree(tgroup);
    if (name)
        kfree(name);
    if (queue)
        queue_free(queue);
    printk("failed to create task group, error: %d\n", err);
    return err;
}

void tgroup_wait_all(tgroup_t *tgroup)
{
    thread_t *thread = NULL;
    queue_node_t *next = NULL;

    if (tgroup == NULL)
        return;
    
    queue_lock(tgroup->queue);

    forlinked(node, tgroup->queue->head, next)
    {
        next = node->next;
        thread = node->data;

        if (thread == current)
            continue;

        thread_lock(thread);

        thread_kill_n(thread);
        thread_wake(thread);
        
        queue_unlock(tgroup->queue);
        thread_wait(thread, 0, NULL);
        queue_lock(tgroup->queue);

        thread_unlock(thread);
    }

    queue_unlock(tgroup->queue);

    return;
}

int tgroup_kill_thread(tgroup_t *tgroup, tid_t tid)
{
    int err = 0;
    thread_t *thread = NULL;
    queue_node_t *next = NULL;

    if (tgroup == NULL)
        return -EINVAL;
    
    if (current && thread_iskilled(current))
        return -EALREADY;

    if (tid == -1)
        goto all;
    else if (tid == 0)
        thread_exit(0);

    if ((err = thread_get(tgroup, tid, &thread)))
        return err;

    thread_assert_lock(thread);
    if (thread == current)
    {
        current_unlock();
        thread_exit(0);
    }

    if ((err = thread_kill_n(thread)))
    {
        thread_unlock(thread);
        return err;
    }
    thread_unlock(thread);

    return 0;
all:
    queue_lock(tgroup->queue);

    forlinked(node, tgroup->queue->head, next)
    {
        next = node->next;
        thread = node->data;

        if (thread == current)
            continue;

        thread_lock(thread);

        thread_kill_n(thread);
        thread_wake(thread);

        queue_unlock(tgroup->queue);
        thread_wait(thread, 1, NULL);
        queue_lock(tgroup->queue);

        thread_unlock(thread);
    }

    queue_unlock(tgroup->queue);

    return 0;
}

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
    vmr_t *stack = NULL;
    int err = 0, argc = 0;
    char **argv = NULL, **envv = NULL;

    proc_assert_lock(proc);
    thread_assert_lock(thread);
    mmap_assert_locked(proc->mmap);

    if ((err = argenv_copy(proc->mmap, (const char **)argp,
                           (const char **)envp, &argv, &argc, &envv)))
        goto error;

    stack = thread->t_tarch->ustack;
    if ((err = mmap_alloc_stack(proc->mmap, USTACKSIZE, &thread->t_tarch->ustack)))
    {
        thread->t_tarch->ustack = stack;
        goto error;
    }

    if ((err = arch_uthread_init(thread->t_tarch, entry, (const char **)argv, argc, (const char **)envv)))
    {
        thread->t_tarch->ustack = stack;
        goto error;
    }

    return 0;
error:
    if (stack)
        mmap_remove(proc->mmap, stack);
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

    if (!(vmr = mmap_find(dst->mmap, ((trapframe_t *)src->t_tarch->tf)->esp)))
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