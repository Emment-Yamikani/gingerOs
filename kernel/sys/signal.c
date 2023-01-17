#include <sys/session.h>
#include <sys/_signal.h>
#include <printk.h>
#include <arch/context.h>
#include <lib/string.h>
#include <lime/string.h>
#include <bits/errno.h>
#include <mm/kalloc.h>

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
        cond_wait(signals->sig_wait);
    
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
int signal_proc_send(struct proc *proc, int signal);
int signal_pgrp_send(PGROUP pg, int signal);

void (*signal(int sig, void (*handler)(int)))(int)
{
}

int kill(void)
{
}

int pause(void)
{
}