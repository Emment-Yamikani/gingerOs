#include <sys/proc.h>
#include <mm/kalloc.h>
#include <printk.h>
#include <lib/string.h>
#include <lib/stddef.h>
#include <lime/string.h>
#include <bits/errno.h>
#include <sys/thread.h>
#include <fs/fs.h>
#include <arch/i386/paging.h>
#include <arch/sys/proc.h>
#include <arch/sys/uthread.h>
#include <sys/binfmt.h>
#include <sys/session.h>

proc_t *initproc = NULL;
queue_t *processes = QUEUE_NEW("All Processes");

pid_t pids = 0;
spinlock_t *pids_lock = SPINLOCK_NEW("proc-ID-lock");

int proc_alloc(const char *name, proc_t **ref)
{
    int err = 0;
    pid_t pid = 0;
    char *str = NULL;
    shm_t *mmap = NULL;
    proc_t *proc = NULL;
    SIGNAL sigs = NULL;
    thread_t *tmain = NULL;
    tgroup_t *tgroup = NULL;
    cond_t *procwait = NULL;
    spinlock_t *lock = NULL;
    file_table_t *ft = NULL;
    queue_t *children = NULL;
    uio_t uio = {.u_cwd = "/"};

    assert(name, "proc name not specified");
    assert(ref, "proc ref not specified");

    if ((err = shm_alloc(&mmap)))
        goto error;

    if ((str = combine_strings(name, "-signals")) == NULL)
    {
        err = -ENOMEM;
        goto error;
    }
    if ((err = signals_alloc(str, &sigs)))
        goto error;
    kfree(str);

    if ((str = combine_strings(name, "-children")) == NULL)
    {
        err = -ENOMEM;
        goto error;
    }
    if ((err = queue_new(str, &children)))
        goto error;
    kfree(str);
    queue_lock(children);

    if ((str = combine_strings(name, "-proc")) == NULL)
    {
        err = -ENOMEM;
        goto error;
    }
    if ((err = spinlock_init(NULL, str, &lock)))
        goto error;
    kfree(str);

    if ((str = combine_strings(name, "-proc")) == NULL)
    {
        err = -ENOMEM;
        goto error;
    }
    if ((err = cond_init(NULL, str, &procwait)))
        goto error;
    kfree(str);

    if ((err = thread_new(&tmain)))
        goto error;

    if ((err = tgroup_new(tmain->t_tid, &tgroup)))
        goto error;

    if ((err = thread_enqueue(tgroup->queue, tmain, NULL)))
        goto error;

    atomic_incr(&tgroup->nthreads);
    tmain->t_gid = tmain->t_tid;

    if ((err = f_alloc_table(&ft)))
        goto error;

    ft->uio = uio;
    if ((str = strdup(name)) == NULL)
    {
        err = -ENOMEM;
        goto error;
    }

    if ((proc = kmalloc(sizeof *proc)) == NULL)
    {
        err = -ENOMEM;
        goto error;
    }

    memset(proc, 0, sizeof *proc);
    spin_lock(pids_lock);
    pid = ++pids;
    spin_unlock(pids_lock);

    proc->pid = pid;
    proc->name = str;
    proc->lock = lock;
    proc->mmap = mmap;
    proc->ftable = ft;
    proc->tmain = tmain;
    proc->signals = sigs;
    proc->tgroup = tgroup;
    proc->wait = procwait;
    proc->children = children;

    tmain->mmap = mmap;
    tmain->owner = proc;
    tmain->t_file_table = ft;
    tmain->t_group = tgroup;

    queue_lock(processes);
    if (enqueue(processes, proc) == NULL)
    {
        err = -ENOMEM;
        queue_unlock(processes);
        goto error;
    }
    queue_unlock(processes);

    queue_unlock(children);

    *ref = proc;
    return 0;

error:
    if (proc)
        kfree(proc);
    if (mmap)
        shm_free(mmap);
    if (sigs)
        signals_free(sigs);
    if (tmain)
        thread_free(tmain);
    if (children)
        queue_free(children);
    if (tgroup)
        tgroup_free(tgroup);
    if (procwait)
        cond_free(procwait);
    if (lock)
        spinlock_free(lock);
    if (ft)
        f_free_table(ft);
    klog(KLOG_FAIL, "failed to allocate proc struct, error= %d\n", err);
    return err;
}

int proc_init(const char *fn)
{
    int err = 0;
    proc_t *proc = NULL;
    thread_t *thread = NULL;
    uintptr_t oldpgdir = 0;
    const char *argp[] = {fn, NULL};
    const char *envp[] = {"PATH=/", NULL};

    assert(fn, "path to \'init\' not specified");

    if ((err = binfmt_load(fn, NULL, &proc)))
        goto error;

    thread = proc->tmain;

    shm_lock(proc->mmap);
    oldpgdir = arch_proc_init(proc->mmap);

    if ((err = thread_execve(proc, thread, (void *)proc->entry, argp, envp)))
        goto error;

    paging_switch(oldpgdir);
    shm_unlock(proc->mmap);
    initproc = proc;
    sched_set_priority(thread, SCHED_LOWEST_PRIORITY);

    if ((err = sched_park(thread)))
        goto error;

    thread_unlock(thread);
    return 0;
error:
    klog(KLOG_FAIL, "failed to load \'init\', error= %d\n", err);
    return err;
}

int proc_copy(proc_t *dst, proc_t *src)
{
    int err = 0;
    char *path = NULL;

    proc_assert_lock(src);
    proc_assert_lock(dst);
    shm_assert_lock(src->mmap);
    shm_assert_lock(dst->mmap);

    shm_pgdir_lock(dst->mmap);
    shm_pgdir_lock(src->mmap);

    if ((err = shm_copy(dst->mmap, src->mmap)))
    {
        shm_pgdir_unlock(src->mmap);
        shm_pgdir_unlock(dst->mmap);
        goto error;
    }

    shm_pgdir_unlock(src->mmap);
    shm_pgdir_unlock(dst->mmap);

    dst->entry = src->entry;
    dst->state = src->state;

    file_table_lock(src->ftable);
    file_table_lock(dst->ftable);

    for (int i = 0; i < NFILE; ++i)
    {
        fdup(src->ftable->file[i]);
        dst->ftable->file[i] = src->ftable->file[i];
    }

    if (!(path = strdup(src->ftable->uio.u_cwd)))
    {
        err = -ENOMEM;
        file_table_unlock(dst->ftable);
        file_table_unlock(src->ftable);
        goto error;
    }

    dst->ftable->uio.u_cwd = path;
    dst->ftable->uio.u_egid = src->ftable->uio.u_egid;
    dst->ftable->uio.u_euid = src->ftable->uio.u_euid;
    dst->ftable->uio.u_gid = src->ftable->uio.u_gid;
    dst->ftable->uio.u_uid = src->ftable->uio.u_uid;

    file_table_unlock(dst->ftable);
    file_table_unlock(src->ftable);


    /*copy signal despositions*/
    signals_lock(src->signals);
    signals_lock(dst->signals);

    for (int sig = 0; sig < NSIG; ++sig)
        dst->signals->sig_action[sig] = src->signals->sig_action[sig];

    signals_unlock(dst->signals);
    signals_unlock(src->signals);

    if (src->pgroup)
    {
        pgroup_lock(src->pgroup);
        if ((err = pgroup_add(src->pgroup, dst)))
        {
            pgroup_unlock(src->pgroup);
            goto error;
        }

        dst->session = src->session;
        pgroup_unlock(src->pgroup);
    }

    return 0;

error:
    klog(KLOG_FAIL, "failed to copy process, error: %d\n", err);
    return err;
}

void proc_free(proc_t *proc)
{
    if (proc->name)
        kfree(proc->name);

    if (proc->mmap)
        shm_free(proc->mmap);

    if (proc->signals)
        signals_free(proc->signals);

    if (proc->children)
    {
        queue_lock(proc->children);
        queue_free(proc->children);
    }

    if (proc->tgroup)
        tgroup_free(proc->tgroup);

    if (proc->wait)
        cond_free(proc->wait);

    if (proc->ftable)
        f_free_table(proc->ftable);

    if (proc->lock)
        spinlock_free(proc->lock);

    if (proc)
        kfree(proc);
}

int proc_get(pid_t pid, proc_t **ref)
{
    proc_t *p = NULL;
    if (pid <= 0)
        return -EINVAL;

    assert(ref, "no ref to proc");

    queue_lock(processes);
    forlinked(node, processes->head, node->next)
    {
        p = node->data;
        proc_lock(p);
        if (p->pid == pid)
        {
            *ref = p;
            queue_unlock(processes);
            return 0;
        }
        proc_unlock(p);
    }
    queue_unlock(processes);

    return -ESRCH;
}

int proc_has_execed(proc_t *p)
{
    proc_assert_lock(p);
    return (atomic_read(&p->flags) & PROC_EXECED);
}

int proc_iskilled(proc_t *p)
{
    proc_assert_lock(p);
    return (atomic_read(&p->flags) & PROC_KILLED);
}

int proc_isorphan(proc_t *p)
{
    proc_assert_lock(p);
    return (atomic_read(&p->flags) & PROC_ORPHANED);
}