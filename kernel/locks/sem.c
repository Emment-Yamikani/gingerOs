#include <bits/errno.h>
#include <lib/string.h>
#include <locks/semaphore.h>
#include <lime/preempt.h>
#include <locks/barrier.h>
#include <mm/kalloc.h>
#include <printk.h>
#include <sys/sched.h>

int sem_init(sem_t *sem, int _, int init) {
    int err = 0;
    queue_t *queue = NULL;

    if (sem == NULL)
        return -EINVAL;

    if ((err = queue_new("SEM_waiters", &queue)))
        return err;

    memset(sem, 0, sizeof *sem);
    sem->value = init;
    sem->waiters = queue;
    return 0;
}

int sem_new(int init, sem_t **psem) {
    int err = 0;
    sem_t *sem = NULL;

    if (psem == NULL)
        return -EINVAL;

    if ((sem = kmalloc(sizeof *sem)) == NULL)
        return -ENOMEM;

    if ((err = sem_init(sem, 0, init)))
    {
        kfree(sem);
        return err;
    }

    *psem = sem;
    return 0;
}

static void lock_guard(sem_t *sem) {
    assert(sem, "No sem");
    pushcli();
    barrier();
    while (atomic_xchg(&sem->guard, 1)) {
        popcli();
        CPU_RELAX();
        pushcli();
    }
}

static void unlock_guard(sem_t *sem)
{
    assert(sem, "No sem");
    atomic_write(&sem->guard, 0);
    popcli();
}

int sem_wait(sem_t *sem) {
    assert(sem, "No sem");
    lock_guard(sem);
    if ((long)atomic_decr(&sem->value) <= 0) {
        
    }
    return -ENOSYS;
}


void sem_post(sem_t *sem);