#pragma once

#include <sys/thread.h>

int kthread_create(void *(*)(void *), void *, tid_t *, thread_t **);
int kthread_create_join(void *(*)(void *), void *, void **);