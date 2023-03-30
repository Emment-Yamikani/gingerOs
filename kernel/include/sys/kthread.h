#pragma once

#include <sys/thread.h>

#define BUILTIN_THREAD(name, entry, arg)                            \
    __section(".__builtin_thread_entry") void *__CAT(__builtin_thread_entry_, name) = entry; \
    __section(".__builtin_thread_arg")  void *__CAT(__builtin_thread_arg_, name) = (void *)arg;

int start_builtin_threads(int *nthreads, thread_t ***threads);
int kthread_create(void *(*entry)(void *), void *arg, tid_t *__tid, thread_t **);
int kthread_create_join(void *(*)(void *), void *, void **);