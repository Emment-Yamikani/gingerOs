#include <arch/i386/cpu.h>
#include <sys/proc.h>
#include <sys/thread.h>
#include <sys/session.h>
#include <sys/_wait.h>
#include <printk.h>
#include <sys/sysproc.h>

pid_t waitpid(pid_t __pid, int *__stat_loc, int __opt)
{
    int err = 0;
    int no_hang = 0;
    pid_t pid = 0;
    int stat_val = 0;
    int has_children = 0;
    PGROUP pgroup = NULL;
    proc_t *process = NULL;
    queue_t *children = NULL;
    queue_node_t *next = NULL;

    printk("[%d:%d:%d]: waitpid(%d, %p, %d)\n", thread_self(), getpid(), getppid(), __pid, __stat_loc, __opt);

    no_hang = __opt & WNOHANG ? 1 : 0;

    if (__pid == WAIT_MYPGRP)
    {
        proc_lock(proc);
        pgroup = proc->pgroup;
        proc_unlock(proc);
        if (pgroup == NULL)
        {
            err = -ECHILD;
            goto error;
        }
        goto wait_pgroup;
    }
    else if (__pid > 0)
    {
        err = (err = proc_getchild(__pid, &process)) == -ESRCH ? -ECHILD : err;
        if (err)
            goto error;
        goto wait_child;
    }
    else if (__pid == WAIT_ANY)
        goto wait_any;
    else
    {
        if ((err = sessions_find_pgroup(ABS(__pid), &pgroup)))
            goto error;
        goto wait_pgroup;
    }

wait_any:
    loop()
    {
        has_children = 0;
        if (atomic_read(&proc->flags) & PROC_KILLED)
            return -EINTR;

        proc_lock(proc);
        children = proc->children;
        proc_unlock(proc);

        queue_lock(children);

        forlinked(node, children->head, next)
        {
            has_children = 1;
            next = node->next;
            process = node->data;

            proc_lock(process);

            switch (process->state)
            {
            case ZOMBIE:
                __fallthrough;
            case TERMINATED:
                tgroup_wait_all(process->tgroup);
                pid = process->pid;
                stat_val = process->exit_status;

                queue_remove(children, process);

                process->parent = NULL;

                proc_unlock(process);
                proc_free(process);

                queue_unlock(children);

                goto done;
            case STOPPED:
                break;
            case CONTINUED:
            default:
                break;
            }

            proc_unlock(process);
        }

        queue_unlock(children);

        if (no_hang)
            goto done;

        // printk("wait()\n");
        if (atomic_read(&proc->flags) & PROC_KILLED)
            return -EINTR;

        if (has_children)
        {
            if (cond_wait(proc->wait_child))
                return -EINTR;
        }
        else
        {
            return -ECHILD;
        }

        if (atomic_read(&proc->flags) & PROC_KILLED)
            return -EINTR;
    }

wait_child:
    proc_lock(proc);
    children = proc->children;
    proc_unlock(proc);

    loop()
    {
        if (atomic_read(&proc->flags) & PROC_KILLED)
            return -EINTR;

        queue_lock(children);

        proc_lock(process);

        switch (process->state)
        {
        case ZOMBIE:
            __fallthrough;
        case TERMINATED:
            tgroup_wait_all(process->tgroup);
            pid = process->pid;
            stat_val = process->exit_status;

            queue_remove(children, process);

            process->parent = NULL;

            proc_unlock(process);
            proc_free(process);

            queue_unlock(children);

            goto done;
        case STOPPED:
            break;
        case CONTINUED:
        default:
            break;
        }

        proc_unlock(process);

        queue_unlock(children);

        if (no_hang)
            goto done;

        if (atomic_read(&proc->flags) & PROC_KILLED)
            return -EINTR;

        if (cond_wait(proc->wait_child))
            return -EINTR;

        if (atomic_read(&proc->flags) & PROC_KILLED)
            return -EINTR;
    }

wait_pgroup:
    loop()
    {
        if (atomic_read(&proc->flags) & PROC_KILLED)
            return -EINTR;

        pgroup_lock(pgroup);
        pgroup_lock_queue(pgroup);

        forlinked(node, pgroup->pg_procs->head, next)
        {
            next = node->next;
            process = node->data;

            proc_lock(process);

            if ((process->state == ZOMBIE) || process->state == TERMINATED)
            {
                pid = process->pid;
                stat_val = process->exit_status;

                proc_lock(process->parent);

                queue_lock(process->parent->children);
                queue_remove(process->parent->children, process);
                process->parent = NULL;
                queue_unlock(process->parent->children);

                proc_unlock(process->parent);

                tgroup_wait_all(process->tgroup);

                proc_unlock(process);
                proc_free(process);

                pgroup_unlock_queue(pgroup);

                goto done;
            }

            proc_unlock(process);
        }

        pgroup_unlock_queue(pgroup);
    }

done:
    if (__stat_loc)
        *__stat_loc = stat_val;
    if (process)
        return pid;
    else
        return -ECHILD;
error:
    return err;
}

pid_t wait(int *__stat_loc)
{
    return waitpid(WAIT_ANY, __stat_loc, 0);
}