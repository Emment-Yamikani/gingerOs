#ifndef THREAD_H
#define THREAD_H

#include "types.h"

typedef void *(*thread_start_t) (void *);

extern int thread_create(tid_t *__tid, thread_start_t, void *__arg);
extern tid_t thread_self(void);
extern int thread_join(tid_t __tid, void *__res);
extern void thread_exit(void *_exit);
extern void thread_yield(void);

#endif //THREAD_H