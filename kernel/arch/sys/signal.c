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
#include <mm/shm.h>

void arch_thread_stop(void);
void arch_thread_start(void);

void handle_signals(void)
{
    /**
     * Handle any pending signal
     */

    current_lock();

    if (proc)
    {
        int sig = 0;
        proc_lock(proc);
        signals_lock(proc->signals);

        while ((sig = (int)queue_get(proc_signals(proc)->sig_queue)))
            arch_handle_signal(sig);

        signals_unlock(proc->signals);
        proc_unlock(proc);
    }

    current_unlock();
}

int arch_handle_signal(int sig)
{
    uintptr_t handler = 0;

    if ((sig < 0) || (sig >= NSIG))
        return -EINVAL;


    current_assert();
    proc_assert_lock(proc);
    signals_assert_lock(proc->signals);

    handler = proc_signals(proc)->sig_action[sig].sa_handler;

    if (handler == SIG_DFL)
        handler = sig_default_action[sig];

    sig_default_action[sig] = SIG_DFL; // revert to default action

    switch (handler)
    {
    case SIGACT_STOP:
        panic("SIGACT_STOP\n");
        break;
    case SIGACT_IGNORE:
        panic("SIGACT_IGNORE\n");
        break;
    case SIGACT_CONTINUE:
        panic("SIGACT_CONTINUE\n");
        break;
    case SIGACT_ABORT:
        __fallthrough;
    case SIGACT_TERMINATE:
        panic("Default signal action is to terminate\n");
        break;
    }

    atomic_or(&current->t_flags, THREAD_HANDLING_SIGNAL);

    x86_thread_t *thread = current->t_tarch;
    uintptr_t *kstack = NULL, *ustack = NULL;
    trapframe_t *saved_tf = current->t_tarch->tf, *tf = NULL;
    context_t *saved_ctx = thread->context, *cpu_ctx = cpu->context, *ctx = NULL;

    kstack = (uintptr_t *)(thread->context - 1);

    *--kstack = (uint32_t)NULL;
    *--kstack = (uint32_t)arch_thread_stop;
    tf = (trapframe_t *)((uint32_t)kstack - sizeof *tf);

    ustack = (uintptr_t *)(saved_tf->esp);
    *--ustack = sig;
    *--ustack = 0xC000F002;

    tf->ss = (SEG_UDATA << 3) | DPL_USER;
    tf->esp = (uintptr_t)ustack; // miss the old stack frame by (minus 8) ???
    tf->ebp = (uintptr_t)ustack; // miss the old stack frame by (minus 8) ???
    tf->eflags = FL_IF;
    tf->cs = (SEG_UCODE << 3) | DPL_USER;
    tf->eip = (uint32_t)handler;

    tf->gs = (SEG_UDATA << 3) | DPL_USER;
    tf->fs = (SEG_UDATA << 3) | DPL_USER;
    tf->es = (SEG_UDATA << 3) | DPL_USER;
    tf->ds = (SEG_UDATA << 3) | DPL_USER;

    kstack = (uint32_t *)tf;
    *--kstack = (uint32_t)trapret;
    ctx = (context_t *)((uint32_t)kstack - sizeof *ctx);
    ctx->eip = (uint32_t)arch_thread_start;
    ctx->ebp = (uintptr_t)(thread->context - 1);

    thread->tf = tf;
    thread->context = ctx;
    
    printk("handler: %d\n", sig);
    set_tss((uintptr_t)(thread->context - 1), (SEG_KDATA << 3));

    signals_unlock(proc->signals);
    proc_unlock(proc);

    swtch(&cpu->context, thread->context);
    
    current_assert_lock();

    proc_lock(proc);
    signals_lock(proc->signals);

    cpu->context = cpu_ctx;
    thread->tf = saved_tf;
    thread->context = saved_ctx;

    atomic_and(&current->t_flags, ~THREAD_HANDLING_SIGNAL);

    printk("handler is done\n");

    return 0;
}

void arch_return_signal(void)
{
    current_lock();
    if ((atomic_read(&current->t_flags) & THREAD_HANDLING_SIGNAL) == 0)
        panic("thread not handling any signal\n");
    
    swtch(&current->t_tarch->context, cpu->context);
}