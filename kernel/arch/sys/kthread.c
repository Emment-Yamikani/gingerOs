#include <lime/string.h>
#include <mm/kalloc.h>
#include <printk.h>
#include <sys/sched.h>
#include <sys/thread.h>
#include <arch/system.h>
#include <arch/sys/uthread.h>
#include <arch/i386/paging.h>
#include <bits/errno.h>
#include <lib/string.h>

void arch_thread_start(void)
{
    //printk("thread(%d) starting...\n", thread_self());
    //printk("\e[0;13mTID(%d) running [%p]\e[0m\n", current->t_tid, return_address(0));
    current_unlock();
}

void arch_thread_stop(void)
{
    arch_thread_exit(read_eax());
}

int arch_kthread_init(x86_thread_t *thread, void *(*entry)(void *), void *arg)
{
    context_t *ctx = NULL;
    ktrapframe_t *tf = NULL;
    uint32_t *kstack = NULL;

    assert(entry, "no entry point");
    assert(thread, "no arch thread");
    
    kstack = __cast_to_type(kstack)(thread->kstack + KSTACKSIZE);
    *--kstack = __cast_to_type(*kstack)arg;
    *--kstack = __cast_to_type(*kstack)arch_thread_stop;

    tf = __cast_to_type(tf)((uint32_t)kstack - sizeof *tf);
    tf->eflags = FL_IF;
    tf->cs = (SEG_KCODE << 3);
    tf->eip = __cast_to_type(tf->eip)entry;
    tf->gs = (SEG_KCPU << 3);
    tf->fs = (SEG_KDATA << 3);
    tf->es = (SEG_KDATA << 3);
    tf->ds = (SEG_KDATA << 3);

    kstack = __cast_to_type(kstack)((uint32_t)tf);
    *--kstack = __cast_to_type(*kstack)trapret;
    ctx = __cast_to_type(ctx)((uint32_t)kstack - sizeof *ctx);
    ctx->eip = __cast_to_type(ctx->eip)arch_thread_start;
    ctx->ebp = __cast_to_type(*kstack)(thread->kstack + KSTACKSIZE);
    
    thread->tf = tf;
    thread->context = ctx;
    return 0;
}

//############################## USER THREADS ###############################

int arch_uthread_init(x86_thread_t *thread, void *(*entry)(void *), const char *argp[], const char *envp[])
{
    int err = 0;
    vmr_t *stack = NULL;
    uintptr_t ustack = 0;
    context_t *ctx = NULL;
    trapframe_t *tf = NULL;
    uint32_t *kstack = NULL;

    assert(thread, "no thread->t_tarch");
    assert(entry, "no entry point");

    stack = thread->ustack;
    ustack = stack->vaddr + USTACKSIZE;
    kstack = (uint32_t *)(thread->kstack + KSTACKSIZE);

    //klog(KLOG_WARN, "\e[0;013mOnly map part of user stack that is acutally in use\e[0m\n");

    size_t mapsz =  2 * sizeof (char *) + 5 * sizeof (uint32_t);

    if (argp)
        foreach (arg, argp)
            mapsz += strlen(arg) + 1 + sizeof(char *);
    if (envp)
        foreach (env, envp)
            mapsz += strlen(env) + 1 + sizeof(char *);
    
    //printk("mapsize: %d\n", mapsz);

    if ((err = paging_mappages(ustack - GET_BOUNDARY_SIZE(0, mapsz), GET_BOUNDARY_SIZE(0, mapsz), stack->vflags)))
        goto error;

    if ((err = arch_uthread_execve(&ustack, argp, envp)))
        goto error;


    *--kstack = (uint32_t)NULL;
    *--kstack = (uint32_t)arch_thread_stop;
    tf = (trapframe_t *)((uint32_t)kstack - sizeof *tf);

    tf->ss = (SEG_UDATA << 3) | DPL_USER;
    tf->esp = ustack;
    tf->ebp = ustack;
    tf->eflags = FL_IF;
    tf->cs = (SEG_UCODE << 3) | DPL_USER;
    tf->eip = (uint32_t)entry;

    tf->gs = (SEG_UDATA << 3) | DPL_USER;
    tf->fs = (SEG_UDATA << 3) | DPL_USER;
    tf->es = (SEG_UDATA << 3) | DPL_USER;
    tf->ds = (SEG_UDATA << 3) | DPL_USER;

    kstack = (uint32_t*)tf;
    *--kstack = (uint32_t)trapret;
    ctx = (context_t *)((uint32_t)kstack - sizeof *ctx);
    ctx->eip = (uint32_t)arch_thread_start;
    ctx->ebp = (uint32_t)(thread->kstack + KSTACKSIZE);

    thread->tf = tf;
    thread->context = ctx;
    return 0;
error:
    return err;
}

int arch_uthread_create(x86_thread_t *thread, void *(*entry)(void *), void *arg)
{
    int err = 0;
    vmr_t *stack = NULL;
    context_t *ctx = NULL;
    trapframe_t *tf = NULL;
    uint32_t *ustack = NULL;
    uint32_t *kstack = NULL;

    assert(thread, "no thread->t_tarch");
    assert(entry, "no entry point");

    stack = thread->ustack;
    ustack = (uint32_t *)(stack->vaddr + USTACKSIZE);
    kstack = (uint32_t *)(thread->kstack + KSTACKSIZE);

    if ((err = paging_mappages(((uintptr_t)ustack) - PAGESZ, PAGESZ, stack->vflags)))
        goto error;

    *--ustack = (uint32_t)arg;
    *--ustack = (uint32_t)0xC0DC0FFE; // dummy return address for user-threads

    *--kstack = (uint32_t)NULL;
    *--kstack = (uint32_t)arch_thread_stop;
    tf = (trapframe_t *)((uint32_t)kstack - sizeof *tf);

    tf->ss = (SEG_UDATA << 3) | DPL_USER;
    tf->esp = (uint32_t)ustack;
    tf->ebp = (uint32_t)ustack;
    tf->eflags = FL_IF | 2;
    tf->cs = (SEG_UCODE << 3) | DPL_USER;
    tf->eip = (uint32_t)entry;

    tf->gs = (SEG_UDATA << 3) | DPL_USER;
    tf->fs = (SEG_UDATA << 3) | DPL_USER;
    tf->es = (SEG_UDATA << 3) | DPL_USER;
    tf->ds = (SEG_UDATA << 3) | DPL_USER;

    kstack = (uint32_t *)tf;
    *--kstack = (uint32_t)trapret;
    ctx = (context_t *)((uint32_t)kstack - sizeof *ctx);
    ctx->eip = (uint32_t)arch_thread_start;
    ctx->ebp = (uint32_t)(thread->kstack + KSTACKSIZE);

    thread->tf = tf;
    thread->context = ctx;

error:
    return err;
}

int arch_thread_fork(x86_thread_t *dst, x86_thread_t *src)
{
    context_t *ctx = NULL;
    trapframe_t *tf = NULL;
    uint32_t *kstack = NULL;

    assert(src, "no source arch_thread");
    assert(dst, "no destination arch_thread");

    kstack = __cast_to_type(kstack)(dst->kstack + KSTACKSIZE);
    
    *--kstack = (uint32_t)NULL;
    *--kstack = (uint32_t)arch_thread_stop;
    tf = (trapframe_t *)((uint32_t)kstack - sizeof *tf);
    *tf = *((trapframe_t *)src->tf);
    tf->eax = 0;

    kstack = (uint32_t *)tf;
    *--kstack = (uint32_t)trapret;
    ctx = (context_t *)((uint32_t)kstack - sizeof *ctx);
    ctx->eip = (uint32_t)arch_thread_start;
    ctx->ebp = (uint32_t)(dst->kstack + KSTACKSIZE);

    dst->tf = tf;
    dst->context = ctx;

    return 0;
}