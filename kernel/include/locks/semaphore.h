#pragma once

#include <ds/queue.h>
#include <sys/thread.h>

typedef struct __sem
{
    atomic_t value;
    thread_t *thread;
    queue_t *waiters;
    atomic_t *guard;
} sem_t;

int sem_new(int init, sem_t **psem);
int sem_init(sem_t *sem, int _, int init);
int sem_wait(sem_t *sem);
void sem_post(sem_t *sem);