#pragma once

#include <arch/sys/thread.h>

int arch_uthread_create(x86_thread_t *thread, void *(*entry)(void *), void *arg);
int arch_uthread_init(x86_thread_t *thread, void *(*entry)(void *), const char *argp[], const char *envp[]);
int arch_uthread_execve(uintptr_t *, const char **, const char **);
char ***arch_execve_cpy(char *_argp[], char *_envp[]);
void arch_exec_free_cpy(char ***arg_env);