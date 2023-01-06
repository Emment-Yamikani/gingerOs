#include <arch/sys/thread.h>
#include <bits/errno.h>
#include <lib/string.h>
#include <lime/string.h>
#include <locks/spinlock.h>
#include <mm/kalloc.h>
#include <printk.h>
#include <sys/thread.h>
#include <sys/kthread.h>
#include <arch/sys/kthread.h>
#include <fs/fs.h>

void *kthread_main(void *);
static tgroup_t *kthreads = NULL;
static struct file_table *file_table = &(struct file_table){0};

int kthread_create(void *(*entry)(void *), void *arg, tid_t *tref, thread_t **ref)
{
    int err =0;
    thread_t *thread = NULL;

    if ((err = thread_new(&thread)))
        goto error;

    if ((err = arch_kthread_init(thread->t_tarch, entry, arg)))
        goto error;

    if (kthreads == NULL)
    {
        assert(!(err = f_alloc_table(&file_table)), "couldn't allocate file table");
        assert(!(err = tgroup_new(thread->t_tid, &kthreads)), "failed to create tgroup");
    }

    if ((err = thread_enqueue(kthreads->queue, thread, NULL)))
        goto error;

    thread->t_group = kthreads;
    thread->t_file_table = file_table;

    if (ref) *ref = thread;
    if (tref) *tref = thread->t_tid;

    sched_set_priority(thread, SCHED_LOWEST_PRIORITY);

    err = sched_park(thread);
    thread_unlock(thread);

    return err;
error:
    panic("failed to create kthread, error=%d\n", err);
    return err;
}

int kthread_create_join(void *(*entry)(void *), void *arg, void **ret)
{
    int err = 0;
    assert(entry, "no entry point");
    tid_t tid = 0;
    if ((err = kthread_create(entry, arg, &tid, NULL)))
        return err;
    return thread_join(tid, ret);
}