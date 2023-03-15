#pragma once

#include <arch/context.h>
#include <lib/stdint.h>
#include <mm/mmap.h>

/*x86 32bit architecture specific thread*/
typedef struct x86_thread
{
    void *tf;           /*task's trapframe*/
    context_t *context; /*for context switching*/
    trapframe_t savedtf;
    uintptr_t kstack;   /*task's kernel stack*/
    vmr_t *ustack;      /*task's allocated user stack, 0 for kernel tasks*/
    void *private;
} x86_thread_t;

int arch_thread_alloc(x86_thread_t **);
void arch_thread_free(x86_thread_t *);

int arch_thread_fork(x86_thread_t *, x86_thread_t *);

void arch_thread_exit(uintptr_t);
void thread_setkstack(x86_thread_t *thread);