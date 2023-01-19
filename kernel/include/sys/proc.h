#pragma once

#include <mm/shm.h>
#include <fs/fs.h>
#include <ds/queue.h>
#include <lib/types.h>
#include <lib/stdint.h>
#include <locks/cond.h>
#include <sys/_signal.h>
#include <locks/atomic.h>
#include <locks/spinlock.h>

typedef struct tgroup tgroup_t;
struct session;
struct pgroup;

#define PROC_KILLED     0x01
#define PROC_EXECED     0x02
#define PROC_ORPHANED   0x04
#define PROC_REAP       0x08

typedef struct proc
{
    pid_t pid;
    char *name;
    shm_t *mmap;
    uintptr_t entry;
    struct proc *parent;
    file_table_t *ftable;
    atomic_t flags;
    pid_t *killer;
    uintptr_t exit;

    enum
    {
        EMBRYO,
        SLEEPING,
        RUNNING,
        ZOMBIE,
    } state;

    thread_t *tmain;
    tgroup_t *tgroup;

    cond_t *wait;

    struct signals *signals;

    queue_t *children;

    struct pgroup *pgroup;
    struct session *session;

    spinlock_t *lock;
} proc_t;

extern proc_t *initproc;
extern queue_t *processes;

#define proc_assert(p) assert(p, "no proc")
#define proc_assert_lock(p)        \
    {                              \
        proc_assert(p);            \
        spin_assert_lock(p->lock); \
    }
#define proc_lock(p)              \
    {                             \
        proc_assert(p);           \
        spin_lock(p->lock);       \
        if (LIME_DEBUG) printk("%s:%d locked(%d)\n", __FILE__, __LINE__, p->pid); \
    }
#define proc_unlock(p)        \
    {                         \
        proc_assert(p);       \
        spin_unlock(p->lock); \
        if (LIME_DEBUG) printk("%s:%d unlocked(%d)\n", __FILE__, __LINE__, p->pid); \
    }

int proc_has_execed(proc_t *);
int proc_iskilled(proc_t *);
int proc_isorphan(proc_t *);

int proc_get(pid_t, proc_t **);
void proc_free(proc_t *);
int proc_init(const char *);
int proc_copy(proc_t *, proc_t *);
int proc_alloc(const char *, proc_t **);
