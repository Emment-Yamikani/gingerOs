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

void exit(int status)
{
    proc_t *child = NULL;
    proc_t *parent = NULL;
    SESSION session = NULL;
    queue_node_t *next = NULL;

    proc_assert(proc);

    if (proc == initproc)
        panic("init exiting\n");

    if (thread_kill_all() == -ERFKILL)
    {
        thread_exit(status);
    }

    proc_lock(proc);
    parent = proc->parent;
    session = proc->session;

    proc_lock(parent);

    printk("EXIT_SYSCALL\n");

    queue_lock(processes);
    queue_remove(processes, proc);
    queue_unlock(processes);

    if (session)
        session_exit(session, proc, 1);

    for (int fd = 0; fd < NFILE; ++fd)
        close(fd);

    klog(KLOG_WARN, "cancel pending signals\n");

    queue_lock(initproc->children);
    queue_lock(proc->children);

    forlinked(node, proc->children->head, next)
    {
        next = node->next;
        child = node->data;
        proc_lock(child);

        child->parent = initproc;
        queue_remove(proc->children, child);
        atomic_or(&child->flags, PROC_ORPHANED);
        enqueue(initproc->children, child);
        proc_unlock(child);
    }

    queue_unlock(proc->children);
    queue_unlock(initproc->children);

    shm_lock(proc->mmap);
    shm_pgdir_lock(proc->mmap);
    shm_unmap_addrspace(proc->mmap);
    shm_pgdir_unlock(proc->mmap);
    shm_unlock(proc->mmap);

    proc->state = ZOMBIE;
    proc->exit = status;

    cond_broadcast(proc->wait);
    cond_broadcast(parent->wait);

    printk("[\e[0;11m%d\e[0m:\e[0;012m%d\e[0m:\e[0;02m%d\e[0m]: '%s' "
           "\e[0;04mexit(\e[0;017m%d\e[0m)\e[0m\n",
           parent->pid, proc->pid, thread_self(),  proc->name, status);

    proc_unlock(parent);
    proc_unlock(proc);

    thread_exit(status);
    panic("exit unsuccessful\n");
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
    shm_lock(proc->mmap);

    if ((err = proc_alloc(proc->name, &child)))
    {
        shm_unlock(proc->mmap);
        proc_unlock(proc);
        goto error;
    }

    proc_lock(child);
    shm_lock(child->mmap);

    if ((err = proc_copy(child, proc)))
    {
        shm_unlock(child->mmap);
        proc_unlock(child);
        shm_unlock(proc->mmap);
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

        shm_unlock(child->mmap);
        proc_unlock(child);
        shm_unlock(proc->mmap);
        proc_unlock(proc);
        goto error;
    }

    current_unlock();

    shm_unlock(child->mmap);
    proc_unlock(child);

    shm_unlock(proc->mmap);
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

pid_t wait(int *staloc)
{
    pid_t pid = 0;
    proc_t *child = NULL;
    atomic_t haskids;
    queue_node_t *next = NULL;

    for (;;)
    {
        atomic_clear(&haskids);

        proc_lock(proc);
        queue_lock(proc->children);

        forlinked(node, proc->children->head, next)
        {
            atomic_incr(&haskids);
            next = node->next;
            child = node->data;
            proc_lock(child);
            if (child->state == ZOMBIE)
            {
                queue_remove(proc->children, child);

                *staloc = child->exit;
                pid = child->pid;

                assert(child->tgroup, "No tgroup");
                while (atomic_read(&child->tgroup->nthreads))
                {
                    proc_unlock(child);
                    CPU_RELAX();
                    proc_lock(child);
                }

                proc_unlock(child);
                proc_free(child);
                queue_unlock(proc->children);
                proc_unlock(proc);
                goto done;
            }
            proc_unlock(child);
        }

        queue_unlock(proc->children);

        if (atomic_read(&proc->flags) & PROC_KILLED)
        {
            proc_unlock(proc);
            return -EINTR;
        }

        if (atomic_read(&haskids))
        {
            proc_unlock(proc);
            if (cond_wait(proc->wait))
                return -EINTR;
            continue;
        }
        else
        {
            proc_unlock(proc);
            return -EINVAL;
        }
        proc_unlock(proc);
    }

done:
    return pid;
}

int execve(char *path, char *argp[], char *envp[])
{
    int err = 0;
    char ***argp_envp = NULL;

    if (!(path = strdup(path)))
    {
        err = -ENOMEM;
        goto error;
    }

    // printk("execve(%s, %p, %p)\n", path, argp, envp);

    if (!(argp_envp = arch_execve_cpy(argp, envp)))
    {
        kfree(path);
        err = -ENOMEM;
        goto error;
    }

    if ((err = binfmt_load(path, current->owner, NULL)))
    {
        kfree(path);
        arch_exec_free_cpy(argp_envp);
        goto error;
    }

    current->t_tarch->ustack = NULL;

    shm_lock(proc->mmap);
    if ((err = thread_execve(proc, current, (void *)proc->entry, (const char **)argp_envp[0], (const char **)argp_envp[1])))
    {
        shm_unlock(proc->mmap);
        kfree(path);
        arch_exec_free_cpy(argp_envp);
        goto error;
    }
    shm_unlock(proc->mmap);

    arch_exec_free_cpy(argp_envp);

    cli();
    context_t *ctx = NULL;

    current_lock();
    proc->name = path;
    current->t_state = T_READY;

    atomic_or(&proc->flags, PROC_EXECED);

    swtch(&ctx, cpu->context);
    panic("\e[014;0mexecve-1\e[0m\n");
    return 0;
error:
    return err;
}

int execv(char *path, char *argp[])
{
    return execve(path, argp, NULL);
}

int brk(void *addr);

void *sbrk(ptrdiff_t nbytes)
{
    int err = 0;
    vmr_t *vmr = NULL;

    shm_lock(proc->mmap);
    shm_pgdir_lock(proc->mmap);

    uintptr_t _brk = proc->mmap->brk_start;

    if (!nbytes)
    {
        shm_pgdir_unlock(proc->mmap);
        shm_unlock(proc->mmap);
        return (void *)_brk;
    }

    if ((_brk + nbytes) >= USTACK_LIMIT)
    {
        shm_pgdir_unlock(proc->mmap);
        shm_unlock(proc->mmap);
        return NULL;
    }

    // printk("sbrk(%d): BRK: %p\n", nbytes, _brk);

    vmr = shm_lookup(proc->mmap, _brk - 1);

    if (vmr == NULL)
        goto creat;
    else if (vmr->oflags & VM_GROWSUP)
    {
        if (nbytes > 0)
            vmr->size += (size_t)nbytes;
        else
            vmr->size -= (size_t)nbytes;
        goto done;
    }

creat:
    if ((err = vmr_alloc(&vmr)))
    {
        shm_pgdir_unlock(proc->mmap);
        shm_unlock(proc->mmap);
        goto error;
    }

    vmr->oflags = VM_WRITE | VM_READ | VM_GROWSUP;
    vmr->size = nbytes;
    vmr->vaddr = _brk;
    vmr->vflags = VM_URW;

    // printk("brk: vmr: %p, oflags: \e[0;013m0%b\e[0m, vflags: \e[0;012m0%b\e[0m\n", vmr->vaddr, vmr->oflags, vmr->vflags);

    if ((err = shm_map(proc->mmap, vmr)))
    {
        shm_pgdir_unlock(proc->mmap);
        shm_unlock(proc->mmap);
        goto error;
    }

done:

    if (nbytes > 0)
    {
        proc->mmap->brk_start += (size_t)nbytes;
        proc->mmap->brk_size -= (size_t)nbytes;
    }
    else
    {
        proc->mmap->brk_start -= (size_t)nbytes;
        proc->mmap->brk_size += (size_t)nbytes;
    }

    shm_pgdir_unlock(proc->mmap);
    shm_unlock(proc->mmap);

    return (void *)_brk;

error:
    if (vmr)
        vmr_free(vmr);
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