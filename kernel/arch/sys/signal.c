#include <arch/context.h>
#include <arch/sys/signal.h>
#include <bits/errno.h>
#include <sys/system.h>
#include <sys/proc.h>
#include <sys/thread.h>
#include <arch/i386/cpu.h>
#include <sys/sched.h>
#include <arch/system.h>
#include <sys/system.h>
#include <sys/sysproc.h>
#include <mm/mmap.h>
#include <mm/kalloc.h>
#include <arch/i386/paging.h>
#include <lib/string.h>

void arch_thread_stop(void);
void arch_thread_start(void);

int signal_cancel_desp(SIGNAL signals, int sig)
{
    signals_assert_lock(signals);
    if (signal_isvalid(sig) == 0)
        return -EINVAL;
    signals->sig_action[sig - 1].sa_handler = SIG_DFL;
    return 0;
}

int signals_cancel(struct proc *process)
{
    queue_node_t *next = NULL;
    proc_assert_lock(process);
    
    signals_lock(process->signals);

    signals_lock_queue(process->signals);
    forlinked(node, proc_signals(process)->sig_queue->head, next)
    {
        next = node->next;
        queue_remove_node(proc_signals(process)->sig_queue, node);
    }
    signals_unlock_queue(process->signals);

    for (int sig = 0; sig < NSIG; ++sig)
        proc_signals(process)->sig_action[sig].sa_handler = SIG_DFL;

    signals_unlock(process->signals);

    return 0;
}

void (*signals_get_handler(SIGNAL signals, int sig))(int)
{
    void (*handler)(int) = NULL;
    signals_assert_lock(signals);
    if (signal_isvalid(sig) == 0)
        return (void *)SIG_ERR;
    handler = __cast_to_type(handler) signals->sig_action[sig - 1].sa_handler;
    // revert to default action
    if (signal_cancel_desp(proc_signals(proc), sig))
        return (void *)SIG_ERR;
   return handler;
}

int handle_signals(trapframe_t *tf)
{
    int err = 0, sig = 0;

    proc_lock(proc);
    signals_lock(proc_signals(proc));

    if (signals_pending(proc) && (__thread_ishandling_signal(current) == 0))
    {
        sig = signals_next(proc);
        arch_handle_signal(sig, tf);
    }

    signals_unlock(proc_signals(proc));
    proc_unlock(proc);
    return err;
}

int arch_handle_signal(int sig, trapframe_t *tf)
{
    x86_thread_t *thread = NULL;
    uintptr_t *ustack = NULL, handler = 0;

    handler = (uintptr_t)signals_get_handler(proc_signals(proc), sig);

    if (handler == SIG_DFL)
        handler = (uintptr_t)sig_default_action[sig - 1];

    switch (handler)
    {
    case SIGACT_STOP:
        panic("SIGACT_STOP\n");
        break;
    case SIGACT_IGNORE:
        return 0;
    case SIGACT_CONTINUE:
        panic("SIGACT_CONTINUE: %d\n", sig);
        break;
    case SIGACT_ABORT:
        __fallthrough;
    case SIGACT_TERMINATE:
        signals_unlock(proc_signals(proc));
        proc_unlock(proc);
        exit(sig);
        panic("Default signal action is to terminate\n");
        break;
    }

    current_lock();
    thread = current->t_tarch;
    thread->savedtf = *tf;
    __thread_setflags(current, THREAD_HANDLING_SIGNAL);

    ustack = (uintptr_t *)tf->esp;
    memset(tf, 0, sizeof *tf);

    *--ustack = sig;
    *--ustack = 0xDEADDEAD; // push dummy return pointer

    tf->ss = (SEG_UDATA << 3) | DPL_USER;
    tf->ebp = (uint32_t)ustack;
    tf->esp = (uint32_t)ustack;
    tf->eflags = FL_IF | 2;

    tf->cs = (SEG_UCODE << 3) | DPL_USER;
    tf->eip = handler;
    tf->gs = (SEG_UDATA << 3) | DPL_USER;
    tf->fs = (SEG_UDATA << 3) | DPL_USER;
    tf->es = (SEG_UDATA << 3) | DPL_USER;
    tf->ds = (SEG_UDATA << 3) | DPL_USER;
    tf->temp_esp = thread->savedtf.temp_esp;
    tf->esi = thread->savedtf.esi;
    tf->edi = thread->savedtf.edi;

    current_unlock();
    return 0;
}

void arch_return_signal(trapframe_t *tf)
{
    x86_thread_t *thread = NULL;
    current_lock();
    if (trapframe_isuser(tf) == 0)
        panic("page fault: thread(%d)\n", current->t_tid);
    if (__thread_ishandling_signal(current) == 0)
        panic("thread not handling any signal\n");
    
    thread = current->t_tarch;
    __thread_maskflags(current, THREAD_HANDLING_SIGNAL);
    *tf = thread->savedtf;
    current_unlock();
}