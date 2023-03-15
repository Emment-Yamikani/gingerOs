#include <sys/sysproc.h>
#include <sys/proc.h>
#include <arch/i386/cpu.h>
#include <printk.h>
#include <sys/thread.h>
#include <fs/sysfile.h>
#include <bits/errno.h>
#include <sys/binfmt.h>
#include <arch/system.h>
#include <arch/sys/uthread.h>
#include <mm/kalloc.h>
#include <lib/string.h>
#include <arch/i386/paging.h>
#include <sys/session.h>

pid_t getpid(void)
{
    proc_lock(proc);
    pid_t pid = proc->pid;
    proc_unlock(proc);
    return pid;
}

pid_t getppid(void)
{
    pid_t ppid = 0;
    proc_t *parent = NULL;
    proc_lock(proc);
    parent = proc->parent;
    proc_unlock(proc);

    if (parent == NULL)
        return -1;

    proc_lock(parent);
    ppid = parent->pid;
    proc_unlock(parent);
    return ppid;
}

pid_t fork(void)
{
    int err = 0;
    pid_t pid = 0;
    proc_t *child = NULL;
    thread_t *thread = NULL;

    proc_lock(proc);
    mmap_lock(proc->mmap);

    if ((err = proc_alloc(proc->name, &child)))
    {
        mmap_unlock(proc->mmap);
        proc_unlock(proc);
        goto error;
    }

    proc_lock(child);
    mmap_lock(child->mmap);

    if ((err = proc_copy(child, proc)))
    {
        mmap_unlock(child->mmap);
        proc_unlock(child);
        mmap_unlock(proc->mmap);
        proc_unlock(proc);
        goto error;
    }

    child->parent = proc;
    thread = child->tmain;

    pid = child->pid;

    current_lock();

    if ((err = thread_fork(thread, current)))
    {
        current_unlock();

        mmap_unlock(child->mmap);
        proc_unlock(child);
        mmap_unlock(proc->mmap);
        proc_unlock(proc);
        goto error;
    }

    current_unlock();

    mmap_unlock(child->mmap);
    proc_unlock(child);

    mmap_unlock(proc->mmap);
    proc_unlock(proc);

    queue_lock(proc->children);
    if ((enqueue(proc->children, child)) == NULL)
    {
        err = -ENOMEM;
        queue_unlock(proc->children);
        thread_unlock(thread);
        goto error;
    }
    queue_unlock(proc->children);

    if ((err = sched_park(thread)))
    {
        thread_unlock(thread);
        goto error;
    }

    thread_unlock(thread);
    return pid;
error:
    if (child)
        proc_free(child);
    klog(KLOG_FAIL, "failed to fork a child\n");
    return err;
}

int brk(void *addr);

void *sbrk(ptrdiff_t incr)
{
    int err = 0;
    uintptr_t brk = 0;
    vmr_t *heap = NULL;
    mmap_t *mmap = NULL;
    uintptr_t new_brk = 0;
    size_t heap_max = 0xC000000;
    int decrease = incr < 0 ? 1 : 0;

    if (current == NULL)
        return NULL;

    current_lock();
    mmap = current->mmap;
    current_unlock();

    if (mmap == NULL)
        return NULL;

    mmap_lock(mmap);
    if (mmap->heap == NULL) {
        try:
        if ((ptrdiff_t)heap_max < incr)
            goto error;
        if ((err = mmap_alloc_vmr(mmap, heap_max, PROT_RW, MAP_PRIVATE, &heap)) == -ENOMEM) {
            heap_max /= 1024;
            goto try;
        } else if (err == 0) {
            mmap->heap = heap;
            brk = (mmap->brk = heap->start);
            goto done;
        }
        else
            goto error;
    }

    brk = new_brk = mmap->brk;
    incr = ABS(incr);
    heap = mmap->heap;

    if (decrease) {
        new_brk -= incr;
        if (new_brk < heap->start)
            goto error;
        uintptr_t page = PGROUNDUP(new_brk);
        size_t size = brk - page;
        int np = size / PAGESZ + PGOFFSET(size);
        while (np--)
        {
            paging_unmap(page);
            page += PAGESZ;
        }
    } else {
        new_brk += incr;
        incr = new_brk - heap->end;
        if (new_brk >= __vmr_upper_bound(heap)) {
            if ((err = mmap_vmr_expand(mmap, heap, incr)))
                goto error;
        }
    }

    mmap->brk = new_brk;
done:
    mmap_unlock(mmap);
    return (void *)brk;
error:
    mmap_unlock(mmap);
    return NULL;
}

#include <sys/session.h>

int setpgrp(void)
{
    int err = 0;
    if ((err = setpgid(0, 0)))
        goto error;
    return getpid();
error:
    klog(KLOG_FAIL, "failed to set pgroup\n");
    return err;
}

pid_t getpgrp(void)
{
    return getpgid(0);
}

pid_t getpgid(pid_t pid)
{
    int err = 0;
    pid_t pgid = 0;
    proc_t *p = NULL;

    if (pid < 0)
        return -EINVAL;

    proc_lock(proc);
    if ((pid == 0) || (pid == proc->pid))
        goto self;
    else
        proc_unlock(proc);

    if ((err = proc_get(pid, &p)))
        return err;

    proc_lock(proc);

    if (proc->pgroup == NULL || proc->session == NULL || p->session == NULL || p->pgroup == NULL || (p->session != proc->session))
    {
        proc_unlock(proc);
        proc_unlock(p);
        return -EPERM;
    }

    pgroup_lock(p->pgroup);
    pgid = p->pgroup->pg_id;
    pgroup_unlock(p->pgroup);
    proc_unlock(proc);
    proc_unlock(p);
    goto done;
self:
    if (proc->pgroup == NULL)
    {
        proc_unlock(proc);
        return -EINVAL;
    }
    pgroup_lock(proc->pgroup);
    pgid = proc->pgroup->pg_id;
    pgroup_unlock(proc->pgroup);
    proc_unlock(proc);
done:
    return pgid;
}

int setpgid(pid_t pid, pid_t pgid)
{
    int err = 0;
    PGROUP pgroup = NULL;
    proc_t *leader = NULL;
    SESSION session = NULL;
    proc_t *process = NULL;

    (void)leader;
    (void)pgroup;
    (void)session;

    if ((pid < 0) || (pgid < 0))
        return -EINVAL;

    if (pid == 0) // imply calling process
    {
        proc_lock(proc);
        process = proc;
        pid = proc->pid;
    }
    else if ((err = proc_get(pid, &process)))
        goto error;

    if (pgid == 0)
        pgid = process->pid;

    if (process->parent == proc)
    {
        proc_lock(proc);
        session_lock(proc->session);

        if ((process->session == NULL) || (proc->session == NULL) || (process->session != proc->session))
        {
            session_unlock(proc->session);
            proc_unlock(proc);
            proc_unlock(process);
            err = -EPERM;
            goto error;
        }

        if (proc_has_execed(process))
        {
            session_unlock(proc->session);
            proc_unlock(proc);
            proc_unlock(process);
            err = -EACCES;
            goto error;
        }

        if (issession_leader(proc->session, process))
        {
            session_unlock(proc->session);
            proc_unlock(proc);
            proc_unlock(process);
            err = -EPERM;
            goto error;
        }

        if (pid == pgid)
        {
            if ((err = session_pgroup_join(proc->session, pgid, process)) == -ENOENT)
            {
                if ((err = session_exit(proc->session, process, 0)))
                {
                    session_unlock(proc->session);
                    proc_unlock(proc);
                    proc_unlock(process);
                    goto error;
                }

                if ((err = session_create_pgroup(proc->session, process, &pgroup)))
                {
                    session_unlock(proc->session);
                    proc_unlock(proc);
                    proc_unlock(process);
                    goto error;
                }

                session_unlock(proc->session);
                proc_unlock(proc);
                proc_unlock(process);
                goto done;
            }
            else if (err == 0)
            {
                session_unlock(proc->session);
                proc_unlock(proc);
                proc_unlock(process);
                goto done;
            }
            else
            {
                session_unlock(proc->session);
                proc_unlock(proc);
                proc_unlock(process);
                goto error;
            }
        }

        if ((err = session_pgroup_join(proc->session, pgid, process)) == 0)
        {
            session_unlock(proc->session);
            proc_unlock(proc);
            proc_unlock(process);
            goto done;
        }

        if (err == -ENOENT)
            err = -EPERM;

        /**
         * The value of the pgid argument is valid
         * but does not match the process ID of the process indicated by the pid argument
         * and there is no process with a process group ID that matches
         * the value of the pgid argument in the same session as the calling process.
         */
        session_unlock(proc->session);
        proc_unlock(proc);
        proc_unlock(process);
        goto error;
    }
    else if (process == proc)
    {
        if (proc->session)
        {
            session = proc->session;
            session_lock(session);

            if (pid == pgid)
            {
                if ((err = session_pgroup_join(session, pgid, proc)) == -ENOENT)
                {
                    if ((err = session_exit(session, proc, 0)))
                    {
                        session_unlock(session);
                        proc_unlock(proc);
                        goto error;
                    }

                    if ((err = session_create_pgroup(session, proc, &pgroup)))
                    {
                        session_unlock(session);
                        proc_unlock(proc);
                        goto error;
                    }

                    session_unlock(session);
                    proc_unlock(proc);
                    goto done;
                }
                else if (err == 0)
                {
                    session_unlock(session);
                    proc_unlock(proc);
                    goto done;
                }
                else
                {
                    session_unlock(session);
                    proc_unlock(proc);
                    goto error;
                }
            }
            else
            {
                if ((err = session_pgroup_join(session, pgid, proc)) == 0)
                {
                    session_unlock(session);
                    proc_unlock(proc);
                    goto done;
                }

                if (err == -ENOENT)
                    err = -EPERM;

                /**
                 * The value of the pgid argument is valid
                 * but does not match the process ID of the process indicated by the pid argument
                 * and there is no process with a process group ID that matches
                 * the value of the pgid argument in the same session as the calling process.
                 */
                session_unlock(session);
                proc_unlock(proc);
                goto error;
            }
        }
        else
        {
            if (pid == pgid)
            {
                if ((err = session_create(proc, &session)))
                {
                    proc_unlock(proc);
                    goto error;
                }
                proc_unlock(proc);
                goto done;
            }
            err = -EPERM;
            proc_unlock(proc);
            goto error;
        }
    }
    else
    {
        proc_unlock(process);
        err = -EPERM;
        goto error;
    }

done:
    return 0;

error:
    klog(KLOG_FAIL, "failed to set process pgroup ID, error=%d\n", err);
    return err;
}

pid_t setsid(void)
{
    int err = 0;
    pid_t sid = 0;

    proc_lock(proc);
    sid = proc->pid;

    if (sessions_find_pgroup(proc->pid, NULL) == 0)
    {
        proc_unlock(proc);
        return -EPERM;
    }

    if (proc->session == NULL)
        goto create;

    if ((err = session_exit(proc->session, proc, 1)))
    {
        proc_unlock(proc);
        return err;
    }

create:
    if ((err = session_create(proc, &proc->session)))
    {
        proc_unlock(proc);
        return err;
    }

    proc_unlock(proc);
    return sid;
}

pid_t getsid(pid_t pid)
{
    int err = 0;
    pid_t sid = 0;
    proc_t *process = NULL;

    if ((proc->session == NULL))
        return -EPERM;

    if (pid == 0)
    {
        process = proc;
        proc_lock(proc);
    }
    else if ((err = proc_get(pid, &process)))
        goto error;

    if (proc != process)
        proc_lock(proc);

    if ((process->session == NULL) || (proc->session != process->session))
    {
        proc_unlock(process);
        if (proc != process)
            proc_unlock(proc);
        return -EPERM;
    }
    
    session_lock(process->session);
    sid = process->session->ss_sid;
    session_unlock(process->session);

    proc_unlock(process);
    if (proc != process)
        proc_unlock(proc);

    return sid;
error:
    return err;
}