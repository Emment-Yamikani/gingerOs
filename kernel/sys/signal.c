#include <sys/session.h>
#include <sys/_signal.h>
#include <printk.h>
#include <arch/context.h>
#include <lib/string.h>
#include <lime/string.h>
#include <bits/errno.h>
#include <mm/kalloc.h>
#include <sys/thread.h>
#include <sys/sysproc.h>

int sig_default_action[] = {
    [SIGABRT] = SIGACT_ABORT,
    [SIGALRM] = SIGACT_TERMINATE,
    [SIGBUS] = SIGACT_ABORT,
    [SIGCHLD] = SIGACT_IGNORE,
    [SIGCONT] = SIGACT_CONTINUE,
    [SIGFPE] = SIGACT_ABORT,
    [SIGHUP] = SIGACT_TERMINATE,
    [SIGILL] = SIGACT_ABORT,
    [SIGINT] = SIGACT_TERMINATE,
    [SIGKILL] = SIGACT_TERMINATE,
    [SIGPIPE] = SIGACT_TERMINATE,
    [SIGQUIT] = SIGACT_ABORT,
    [SIGSEGV] = SIGACT_ABORT,
    [SIGSTOP] = SIGACT_STOP,
    [SIGTERM] = SIGACT_TERMINATE,
    [SIGTSTP] = SIGACT_STOP,
    [SIGTTIN] = SIGACT_STOP,
    [SIGTTOU] = SIGACT_STOP,
    [SIGUSR1] = SIGACT_TERMINATE,
    [SIGUSR2] = SIGACT_TERMINATE,
    [SIGPOLL] = SIGACT_TERMINATE,
    //[SIGPROF] = SIGACT_TERMINATE,
    [SIGSYS] = SIGACT_ABORT,
    [SIGTRAP] = SIGACT_ABORT,
    [SIGURG] = SIGACT_IGNORE,
    //[SIGVTALRM] = SIGACT_TERMINATE,
    //[SIGXCPU] = SIGACT_ABORT,
    //[SIGXFSZ] = SIGACT_ABORT,
};

void signals_free(SIGNAL signals)
{
    signals_assert(signals);

    if (signals->sig_queue)
    {
        queue_lock(signals->sig_queue);
        queue_free(signals->sig_queue);
    }

    if (signals->sig_wait)
        cond_free(signals->sig_wait);

    if (signals->sig_lock)
        spinlock_free(signals->sig_lock);

    kfree(signals);
}

int signals_alloc(char *name, SIGNAL *ref)
{
    int err = 0;
    cond_t *wait = NULL;
    queue_t *queue = NULL;
    SIGNAL signals = NULL;
    spinlock_t *lock = NULL;

    assert(name, "no name");
    assert(ref, "no signals reference");

    if ((err = spinlock_init(NULL, name, &lock)))
        goto error;

    if ((err = cond_init(NULL, name, &wait)))
        goto error;

    if ((err = queue_new(name, &queue)))
        goto error;

    queue_lock(queue);

    if ((signals = kmalloc(sizeof *signals)) == NULL)
    {
        err = -ENOMEM;
        goto error;
    }
    queue_unlock(queue);
    memset(signals, 0, sizeof *signals);

    signals->sig_lock = lock;
    signals->sig_queue = queue;
    signals->sig_wait = wait;

    *ref = signals;

    return 0;
error:
    klog(KLOG_FAIL, "failed to alloc signals, error=%d\n", err);
    return err;
}

int signal_send(int pid, int sig);

int signals_pending(struct proc *p)
{
    ssize_t has = 0;
    proc_assert_lock(p);
    signals_assert_lock(proc_signals(p));
    signals_lock_queue(proc_signals(p));
    has = queue_count(proc_signals(p)->sig_queue);
    signals_unlock_queue(proc_signals(p));
    return has;
}

int signals_next(struct proc *p)
{
    proc_assert_lock(p);
    signals_assert_lock(proc_signals(p));
    return (int)queue_get(proc_signals(p)->sig_queue);
}

int signal_proc_send(struct proc *process, int sig)
{
    proc_assert_lock(proc);
    proc_assert_lock(process);

    if ((sig < 1) || (sig >= NSIG))
        return -EINVAL;

    file_table_lock(proc->ftable);
    if ((proc == initproc) || (proc->ftable->uio.u_uid == 0))
    {
        if (proc == initproc)
            goto init;
        else
            goto other;
    }

init:
    if ((process == initproc))
    {
        uintptr_t handler = proc_signals(initproc)->sig_action[sig - 1].sa_handler;
        if ((handler == SIG_DFL) || (handler == SIG_IGN) || (handler == SIG_ERR)){
            file_table_unlock(proc->ftable);
            return -EPERM;
        }
    }

other:
    if (proc != process)
        file_table_lock(process->ftable);

    /**
     * Root can send signals to any process
     * Others processes must be of the same user or group
     * inorder to send signals to one another process.
     */
    if ((proc != process) && (proc->ftable->uio.u_uid) && ((proc->ftable->uio.u_uid != process->ftable->uio.u_uid) && (proc->ftable->uio.u_gid != process->ftable->uio.u_gid)))
    {
        file_table_unlock(process->ftable);
        file_table_unlock(proc->ftable);
        return -EPERM;
    }


    if (proc != process)
        file_table_unlock(process->ftable);

    file_table_unlock(proc->ftable);

    signals_lock(process->signals);
    signals_lock_queue(process->signals);
    if ((enqueue(process->signals->sig_queue, (void *)sig)) == NULL)
    {
        signals_unlock_queue(process->signals);
        signals_unlock(process->signals);
        return -ENOMEM;
    }
    signals_unlock_queue(process->signals);

    cond_broadcast(process->signals->sig_wait);

    signals_unlock(process->signals);

    return 0;
}

int signal_pgrp_send(PGROUP pgroup, int sig, ssize_t *ref)
{
    int err = 0;
    ssize_t count = 0;
    proc_t *process = NULL;
    proc_assert_lock(proc);
    pgroup_assert_lock(pgroup);

    pgroup_lock_queue(pgroup);
    forlinked(node, pgroup->pg_procs->head, node->next)
    {
        process = node->data;
        if (proc != process)
            proc_lock(process);

        if ((err = signal_proc_send(process, sig)))
        {
            if (proc != process)
                proc_unlock(process);
            break;
        }
        if (proc != process)
            proc_unlock(process);
        count++;
    }
    pgroup_unlock_queue(pgroup);

    if (ref)
        *ref = count++;
    return err;
}

void (*signal(int sig, void (*handler)(int)))(int){
    typeof(handler) old_handler = NULL;

    if ((sig < 1) || (sig >= NSIG))
        return __cast_to_type(handler)-EINVAL;

    if ((handler == __cast_to_type(handler) SIG_ERR) ||
        (handler >= __cast_to_type(handler) USTACK_LIMIT))
        return __cast_to_type(handler)-EINVAL;

    proc_lock(proc);
    signals_lock(proc->signals);

    old_handler = __cast_to_type(handler) proc_signals(proc)->sig_action[sig - 1].sa_handler;

    proc_signals(proc)->sig_action[sig - 1].sa_handler = (uintptr_t) handler;

    signals_unlock(proc->signals);
    proc_unlock(proc);

    return old_handler;
}

int kill(pid_t pid, int sig)
{
    int err = 0;
    ssize_t count = 0;
    PGROUP pgroup = NULL;
    proc_t *process = NULL;

    if ((sig < 0) || (sig >= NSIG))
        return -EINVAL;

    if (sig == 0) // check is we have permission to send signals to this process
    {
        proc_lock(proc)
        file_table_lock(proc->ftable);
        if ((proc == initproc) || (proc->ftable->uio.u_uid == 0)){
            file_table_unlock(proc->ftable);
            proc_unlock(proc);
            return 0;
        }

        file_table_unlock(proc->ftable);

        if (pid == -1){
            proc_unlock(proc);
            return 0; // can send sig to aleast itself
        }
        else if (pid == 0)
        {
            if (proc->pgroup == NULL)
            {
                proc_unlock(proc);
                return -EPERM;
            }
            proc_unlock(proc);
            return 0;
        }
        else if (pid > 0)
        {
            if (pid == proc->pid)
                process = proc;
            else if ((err = proc_get(pid, &process)))
            {
                proc_unlock(proc);
                return err;
            }

            file_table_lock(proc->ftable);
            if ((proc == initproc) || (proc->ftable->uio.u_uid == 0))
            {
                file_table_unlock(proc->ftable);
                proc_unlock(proc);
                return 0;
            }

            if ((process == initproc))
            {
                file_table_unlock(proc->ftable);
                if (process != proc)
                    proc_unlock(process);
                proc_unlock(proc);
                return -EPERM;
            }

            if (proc != process)
                file_table_lock(process->ftable);

            /**
             * Root can send signals to any process
             * Others processes must be of the same user or group
             * inorder to send signals to one another process.
             */
            if ((proc != process) && (proc->ftable->uio.u_uid) && ((proc->ftable->uio.u_uid != process->ftable->uio.u_uid) && (proc->ftable->uio.u_gid != process->ftable->uio.u_gid)))
            {
                file_table_unlock(process->ftable);
                file_table_unlock(proc->ftable);
                if (process != proc)
                    proc_unlock(process);
                proc_unlock(proc);
                return -EPERM;
            }

            if (process != proc)
                proc_unlock(process);
            proc_unlock(proc);
            return 0;
        }
        else
        {
            proc_lock(proc);
            if ((err = sessions_find_pgroup(-pid, &pgroup)))
            {
                proc_unlock(proc);
                return err;
            }
            
            if (proc->session != pgroup->pg_sessoin)
            {
                pgroup_unlock(pgroup);
                proc_unlock(proc);
                return -EPERM;
            }
            pgroup_unlock(pgroup);
            proc_unlock(proc);

            return 0;
        }
    }

    if (pid == -1) // send to every process
    {
        queue_lock(processes);
        //ssize_t number = queue_count(processes);
        forlinked(node, processes->head, node->next)
        {
            process = node->data;
            proc_lock(process);
            if ((err = signal_proc_send(process, sig)))
            {
                proc_lock(process);
                queue_lock(processes);
                goto done;
            }
            count++;
            proc_lock(process);
        }
        queue_lock(processes);
    }
    else if (pid == 0) // send a signal to everyone in my pgroup
    {
        proc_lock(proc);
        if (proc->pgroup == NULL){
            proc_unlock(proc);
            return -EINVAL;
        }
        
        pgroup_lock(proc->pgroup);
        err = signal_pgrp_send(proc->pgroup, sig, &count);
        pgroup_unlock(proc->pgroup);
        proc_unlock(proc);
    }
    else if (pid > 0) // send a signal to pid
    {
        if (pid == getpid())
        {
            proc_lock(proc);
            process = proc;
        }
        else if ((err = proc_get(pid, &process)))
            return err;
            
        if (proc != process)
            proc_lock(proc);

        if ((err = signal_proc_send(process, sig)))
        {
            if (process != proc)
                proc_unlock(proc);
            proc_unlock(process);
            return err;
        }

        count = 1;
        if (process != proc)
            proc_unlock(proc);
        proc_unlock(process);
    }
    else // send signal to pgroup
    {
        proc_lock(proc);
        if ((err = sessions_find_pgroup(-pid, &pgroup))){
            proc_unlock(proc);
            return err;
        }

        if (proc->session != pgroup->pg_sessoin)
        {
            pgroup_unlock(pgroup);
            proc_unlock(proc);
            return -EPERM;
        }
        err = signal_pgrp_send(pgroup, sig, &count);
        pgroup_unlock(pgroup);
        proc_unlock(proc);
    }

done:
    if (count)
        return 0;
    else
        return err;
}

int pause(void)
{
    cond_t *cond = NULL;
    atomic_t handled = 0;

    proc_lock(proc);
    signals_lock(proc->signals);

    cond = proc->signals->sig_wait;

    atomic_write(&handled, atomic_read(&proc->signals->nsig_handled));

    while (atomic_read(&handled) <= atomic_read(&proc->signals->nsig_handled))
    {
        signals_unlock(proc->signals);
        proc_unlock(proc);

        if (cond_wait(cond))
            thread_exit(-EINTR);

        proc_lock(proc);
        signals_lock(proc->signals);
    }

    return -EINTR;
}