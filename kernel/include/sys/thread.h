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
#include <mm/mmap.h>

typedef enum
{
    /*Embryo*/
    T_EMBRYO = 0,
    /*Interruptable sleep*/
    T_ISLEEP = 1,
    /*Ready*/
    T_READY = 2,
    /*Running*/
    T_RUNNING = 3,
    /*Zombie*/
    T_ZOMBIE = 4,
    /*thread was stopped*/
    T_STOPPED = 5,
    /*thread was terminated*/
    T_TERMINATED = 6,
} tstate_t;

__unused static const char *STATES[] = {
    [T_EMBRYO] = "embryo",
    [T_READY] = "ready",
    [T_RUNNING] = "running",
    [T_ISLEEP] = "isleep",
    [T_ZOMBIE] = "zombie",
};

typedef struct sched_attr
{
    atomic_t age;         /*thread's total runtime*/
    cpu_t *core;          /*thread's prefered cpu*/
    atomic_t affinity;    /*thread's processor affinity, 0=soft and 1=hard*/
    atomic_t t_priority;  /*thread's priority*/
    atomic_t t_timeslice; /*thread's timeslice*/
} sched_attr_t;

/*default scheduling attributes*/
#define SCHED_ATTR_DEFAULT() (sched_attr_t){0}

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

#define THREAD_USER             0x01
#define THREAD_HANDLING_SIGNAL  0x02
#define THREAD_SETPARK          0x04
#define THREAD_SETWAKEUP        0x08
#define THREAD_KILLED           0x10

typedef struct thread
{
    tid_t t_tid;           /*thread ID*/
    tid_t t_gid;           /*thread group ID*/
    void *fpu_ctx;
    int fpu_enabled;
    tstate_t t_state;      /*thread state*/
    uintptr_t t_exit;      /*thread exit code*/
    atomic_t t_killer;     /*thread killed by other*/
    atomic_t t_flags;      /*thread flags*/
    x86_thread_t *t_tarch; /*thread architecture*/
    proc_t *owner;

    spinlock_t *t_lock;        /*thread lock*/
    sched_attr_t t_sched_attr; /*thread scheduling attributes*/

    mmap_t *mmap;       /*memory map*/
    tgroup_t *t_group; /*thread group*/
    queue_t *t_queues; /*thread queues*/
    struct
    {
        void *data;         /*used to specify what holds the sleep-queue, e.g mutex, condition-variable, etc.*/
        queue_t *queue;     /*thread's sleep queue if sleeping*/
        queue_node_t *node; /*thread's sleep queue node*/
        spinlock_t *guard;  /*non-null if sleep queue is associated with a guard lock*/
    } sleep;

    struct file_table *t_file_table; /*thread file table*/
    cond_t *t_wait;                  /*thread wait condition*/
} thread_t;


#define thread_assert(t) assert(t, "no thread");
#define thread_lock(t) {thread_assert(t); spin_lock(t->t_lock); if (LIME_DEBUG) printk("%s:%d:: TID(%d) locked [%p]\n", __FILE__, __LINE__, t->t_tid, return_address(0));}
#define thread_unlock(t) {thread_assert(t); spin_unlock(t->t_lock); if (LIME_DEBUG) printk("%s:%d:: TID(%d) unlocked [%p]\n", __FILE__, __LINE__, t->t_tid, return_address(0));}
#define thread_assert_lock(t) {thread_assert(t); spin_assert_lock(t->t_lock);}
#define thread_lock_held(thread) ({ thread_assert(thread); spin_holding(thread->t_lock);})

#define __thread_zombie(thread)             ({thread_assert_lock(thread); thread->t_state == T_ZOMBIE; })
#define __thread_isleep(thread)             ({thread_assert_lock(thread); thread->t_state == T_ISLEEP; })
#define __thread_stopped(thread)            ({thread_assert_lock(thread); thread->t_state == T_STOPPED; })
#define __thread_terminated(thread)         ({thread_assert_lock(thread); thread->t_state == T_TERMINATED; })
#define __thread_setflags(thread, flags)    ({thread_assert_lock(thread); atomic_or(&thread->t_flags, (flags)); })
#define __thread_maskflags(thread, flags)   ({thread_assert_lock(thread); atomic_and(&thread->t_flags, ~(flags)); })
#define __thread_testflags(thread, flags)   ({thread_assert_lock(thread); (atomic_read(&thread->t_flags) & (flags)); })
#define __thread_enter_state(thread, state) ({int err = 0; thread_assert_lock(thread); if (((state) < T_EMBRYO) || ((state) > T_TERMINATED)) { err = -EINVAL;} else {thread->t_state = (state);}; err; })
#define __thread_killed(thread)             ({ int held = thread_lock_held(thread); if ((!held)) thread_lock(thread); int killed = __thread_testflags(thread, THREAD_KILLED); if ((!held)) thread_unlock(thread); killed; })
#define __thread_ishandling_signal(thread)  ({ int held = thread_lock_held(thread); if ((!held)) thread_lock(thread); int handling = __thread_testflags(thread, THREAD_HANDLING_SIGNAL); if ((!held)) thread_unlock(thread); handling; })

#define current_assert() assert(current, "no current thread");
#define current_lock() thread_lock(current);
#define current_unlock() thread_unlock(current);
#define current_assert_lock() assert(spin_holding(current->t_lock), "current not locked")
#define current_mmap() (current->mmap)

#define current_mmap_lock()     mmap_lock(current_mmap())
#define current_mmap_unlock()   mmap_unlock(current_mmap())

int thread_new(thread_t **);
void thread_free(thread_t *);
void thread_dump(thread_t *);

void tgroup_free(tgroup_t *);
int tgroup_new(tid_t, tgroup_t **);
void tgroup_wait_all(tgroup_t *tgrp);
int tgroup_kill_thread(tgroup_t *tgroup, tid_t tid);

thread_t *thread_dequeue(queue_t *);
int thread_remove_queue(thread_t *thread, queue_t *queue);
int thread_enqueue(queue_t *, thread_t *, queue_node_t **node);
int queue_get_thread(queue_t *queue, tid_t tid, thread_t **pthread);


void thread_yield(void);
tid_t thread_self(void);
int thread_kill_all(void);
int thread_kill(tid_t tid);
void thread_exit(uintptr_t);
int thread_cancel(tid_t tid);
int thread_wake(thread_t *thread);
int thread_wake_n(thread_t *thread);
int thread_kill_n(thread_t *thread);
int thread_join(tid_t tid, void **retval);
int thread_fork(thread_t *dst, thread_t *src);
int thread_get(tgroup_t *tgrp, tid_t tid, thread_t **tref);
int thread_wait(thread_t *thread, int reap, void **retval);
int thread_create(tid_t *tid, void *(*entry)(void *), void *arg);
int thread_execve(proc_t *proc, thread_t *thread, void *(*entry)(void *), const char *argp[], const char *envp[]);
int argenv_copy(mmap_t *__mmap, const char *__argp[], const char *__envp[], char **argv[], int *pargc, char **envv[]);