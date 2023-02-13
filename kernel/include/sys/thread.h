#pragma once

#include <arch/sys/thread.h>
#include <arch/i386/cpu.h>
#include <ds/queue.h>
#include <lib/types.h>
#include <lib/stdint.h>
#include <lib/stddef.h>
#include <locks/spinlock.h>
#include <sys/system.h>
#include <locks/cond.h>
#include <locks/atomic.h>
#include <lime/assert.h>
#include <mm/shm.h>

typedef enum
{
    /*embryo*/
    T_EMBRYO = 0,
    /*interruptable sleep*/
    T_ISLEEP = 1,
    /*ready*/
    T_READY = 2,
    /*running*/
    T_RUNNING = 3,
    /*zombie*/
    T_ZOMBIE = 4
} tstate_t;

__unused static const char *STATES[] = {
    [T_EMBRYO] = "embryo",
    [T_READY] = "ready",
    [T_RUNNING] = "running",
    [T_ISLEEP] = "isleep",
    [T_ZOMBIE] = "zombie"};

typedef struct sched_attr
{
    atomic_t age;         /*thread's total runtime*/
    cpu_t *core;          /*thread's prefered cpu*/
    atomic_t affinity;    /*thread's processor affinity, 0=soft and 1=hard*/
    atomic_t t_priority;  /*thread's priority*/
    atomic_t t_timeslice; /*thread's timeslice*/
} sched_attr_t;

/*default scheduling attributes*/
#define SCHED_ATTR_DEFAULT() \
    (sched_attr_t)           \
    {                        \
        0                    \
    }

/*soft affinity for the cpu*/
#define SCHED_SOFT_AFFINITY 0

/*hard affinity for the cpu*/
#define SCHED_HARD_AFFINITY 1

typedef struct tgroup
{
    tid_t gid;
    queue_t *queue;
    atomic_t nthreads; // number of active thread(both running and sleeping)
} tgroup_t;

#define THREAD_USER 0x01
#define THREAD_HANDLING_SIGNAL 0x02

typedef struct thread
{
    tid_t t_tid;           /*thread ID*/
    tid_t t_gid;           /*thread group ID*/
    x86_thread_t *t_tarch; /*thread architecture*/
    tstate_t t_state;      /*thread state*/
    uintptr_t t_exit;      /*thread exit code*/
    atomic_t t_killed;     /*thread killed by other*/
    atomic_t t_flags;      /*thread flags*/
    proc_t *owner;

    spinlock_t *t_lock;        /*thread lock*/
    sched_attr_t t_sched_attr; /*thread scheduling attributes*/

    shm_t *mmap;       /*memory map*/
    tgroup_t *t_group; /*thread group*/
    queue_t *t_queues; /*thread queues*/
    struct
    {
        enum
        {
            INVALID,        /*invalid sleep struct*/
            CONDITION,      /*condition variable*/
            MUTEX,          /*mutex*/
        } type;             /*type of queue*/
        void *data;         /*used to specify what holds the sleep-queue, e.g mutex, condition-variable, etc.*/
        queue_t *queue;     /*thread's sleep queue if sleeping*/
        queue_node_t *node; /*thread's sleep queue node*/
    } sleep;

    struct file_table *t_file_table; /*thread file table*/
    cond_t *t_wait;                  /*thread wait condition*/
} thread_t;

#define thread_assert(t) assert(t, "no thread");
#define thread_lock(t) {thread_assert(t); spin_lock(t->t_lock); if (LIME_DEBUG) printk("%s:%d:: TID(%d) locked [%p]\n", __FILE__, __LINE__, t->t_tid, return_address(0));}
#define thread_unlock(t) {thread_assert(t); spin_unlock(t->t_lock); if (LIME_DEBUG) printk("%s:%d:: TID(%d) unlocked [%p]\n", __FILE__, __LINE__, t->t_tid, return_address(0));}
#define thread_assert_lock(t) {thread_assert(t); spin_assert_lock(t->t_lock);}

#define thread_iskilled(t) (atomic_read(&t->t_killed))

#define current_assert() assert(current, "no current thread");
#define current_lock() thread_lock(current);
#define current_unlock() thread_unlock(current);
#define current_assert_lock() assert(spin_holding(current->t_lock), "current not locked")

void thread_free(thread_t *);
void thread_dump(thread_t *);
int thread_new(thread_t **);

void tgroup_free(tgroup_t *);
int tgroup_new(tid_t, tgroup_t **);
thread_t *thread_dequeue(queue_t *);
int thread_enqueue(queue_t *, thread_t *, queue_node_t **node);

int thread_ishandling_signal(thread_t *thread);


void thread_yield(void);
tid_t thread_self(void);
void thread_exit(uintptr_t);
int thread_wake(thread_t *thread);
int thread_fork(thread_t *dst, thread_t *src);
int thread_kill_all(void);
int thread_kill(tid_t tid);
int thread_kill_n(thread_t *thread);
int thread_join(tid_t tid, void **retval);
int thread_create(tid_t *tid, void *(*entry)(void *), void *arg);
int thread_execve(proc_t *proc, thread_t *thread, void *(*entry)(void *), const char *argp[], const char *envp[]);