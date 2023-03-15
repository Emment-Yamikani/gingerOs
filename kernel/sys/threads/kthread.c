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
    int err = 0;
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

    if (ref)
        *ref = thread;
    if (tref)
        *tref = thread->t_tid;

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

extern char __builtin_thread_entry, __builtin_thread_entry_end;
extern char __builtin_thread_arg, __builtin_thread_arg_end;

int start_builtin_threads(int *nthreads, thread_t ***threads)
{
    int nt = 0;
    int err = 0;
    size_t nr = 0;
    thread_t *thread = NULL;
    thread_t **builtin_threads = NULL;

    void **argv = (void **)&__builtin_thread_arg;
    void **entryv = (void **)&__builtin_thread_entry;
    void **end = (void **)&__builtin_thread_entry_end;

    nr = (end - entryv);
    
    for (size_t i = 0; i < nr; ++i)
    {
        void *arg = argv[i];
        void *(*entry)(void *) = entryv[i];

        if (entry)
        {
            if (builtin_threads == NULL)
            {
                if ((builtin_threads = kmalloc(sizeof(thread_t *))) == NULL)
                {
                    err = -ENOMEM;
                    break;
                }
            }
            else if ((builtin_threads = krealloc(builtin_threads, (sizeof(thread_t *) * (nt + 1)))) == NULL)
            {
                err = -ENOMEM;
                break;
            }

            if ((err = kthread_create(entry, arg, NULL, &thread)))
                break;

            builtin_threads[nt++] = thread;
        }
    }

    *nthreads = nt;
    *threads = builtin_threads;
    return err;
}