#pragma once

#include <sys/thread.h>
#include <lib/stdint.h>

int uthread_init(thread_t *, void *(*entry), const char *argp, const char *envp);