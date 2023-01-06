#pragma once

#include <arch/sys/thread.h>

int arch_kthread_init(x86_thread_t *, void *(*)(void *), void *);
void arch_kthread_exit(void);