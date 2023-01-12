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

    proc_assert(proc);

    if (proc == initproc)
        panic("init exiting\n");

    parent = proc->parent;

    // guarante that we're the only ones running
    if (thread_kill_all() == -ERFKILL)
        thread_exit(status);

    // close all open files
    for (int i = 0; i < NFILE; ++i)
        close(i);

    printk("\e[017;0m[%d:%d]\e[0m: '%s' \e[0;04mexit(%d)\e[0m\n", proc->pid, current->t_tid, proc->name, status);
    // orphan all chilren
    queue_lock(proc->children);
    // don't waste time if there is no child to orphan
    if (queue_count(proc->children) == 0)
        goto done_orphaning;

    queue_lock(initproc->children);

    forlinked(node, proc->children->head, node->next)
    {
        child = node->data;
        proc_lock(child);
        queue_remove_node(proc->children, node);
        atomic_write(&child->orphan, 1);

        enqueue(initproc->children, child);
        child->parent = initproc;
        proc_unlock(child);
    }

    queue_unlock(initproc->children);

done_orphaning:
    queue_unlock(proc->children);

    shm_lock(proc->mmap);
    shm_pgdir_lock(proc->mmap);
    shm_unmap_addrspace(proc->mmap);
    shm_pgdir_unlock(proc->mmap);
    shm_unlock(proc->mmap);

    proc_lock(proc);
    proc->exit = status;
    proc->state = ZOMBIE;
    cond_broadcast(proc->wait);   // signal all processes waiting on proc
    cond_broadcast(parent->wait); // signal all processes waiting on parent
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
    atomic_t haskids = 0;

    for (;;)
    {
        atomic_write(&haskids, 0);
        queue_lock(proc->children);

        forlinked(node, proc->children->head, node->next)
        {
            atomic_incr(&haskids);
            child = node->data;
            proc_lock(child);
            if (child->state == ZOMBIE)
            {
                pid = child->pid;
                *staloc = (int)child->exit;
                proc_unlock(child);
                queue_unlock(proc->children);
                while (atomic_read(&child->tgroup->nthreads))
                    ;
                queue_lock(proc->children);
                queue_remove_node(proc->children, node);
                queue_unlock(proc->children);
                proc_free(child);
                goto done;
            }
            proc_unlock(child);
        }

        queue_unlock(proc->children);
        if (atomic_read(&haskids))
            cond_wait(proc->wait);
        else
            return -1;
    }

done:
    //printk("child: %d\n", pid);
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


    //printk("execve(%s, %p, %p)\n", path, argp, envp);

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

    //printk("sbrk(%d): BRK: %p\n", nbytes, _brk);


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

    //printk("brk: vmr: %p, oflags: \e[0;013m0%b\e[0m, vflags: \e[0;012m0%b\e[0m\n", vmr->vaddr, vmr->oflags, vmr->vflags);

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
    pid_t pgid = 0;
    PGROUP pgroup = NULL;
    SESSION session = NULL;

    return -1;
error:
    klog(KLOG_FAIL, "failed to set pgroup\n");
    return err;
}

pid_t setsid(void);
pid_t getsid(pid_t pid);
pid_t getpgid(pid_t pid);
int setpgid(pid_t pid, pid_t pgid);