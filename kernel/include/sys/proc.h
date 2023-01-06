#pragma once

#include <ds/queue.h>
#include <lib/stdint.h>
#include <lib/types.h>
#include <locks/cond.h>
#include <locks/spinlock.h>
#include <mm/shm.h>
#include <locks/atomic.h>
#include <fs/fs.h>

typedef struct tgroup tgroup_t;

typedef struct proc
{
    pid_t pid;
    char *name;
    struct proc *parent;
    uintptr_t entry;
    uintptr_t exit;
    atomic_t killed;
    atomic_t orphan;
    enum {EMBRYO, SLEEPING, RUNNING, ZOMBIE}state;
    shm_t *mmap;
    thread_t *tmain;
    file_table_t *ftable;
    tgroup_t *tgroup;
    queue_t *children;
    queue_t *signals;
    cond_t *wait;
    spinlock_t *lock;
} proc_t;

extern proc_t *initproc;

#define proc_assert(p) assert(p, "no proc")
#define proc_assert_lock(p) {proc_assert(p); spin_assert_lock(p->lock);}
#define proc_lock(p) {proc_assert(p); spin_lock(p->lock);}
#define proc_unlock(p) {proc_assert(p); spin_unlock(p->lock);}

void proc_free(proc_t *);
int proc_init(const char *);
int proc_copy(proc_t *dst, proc_t *src);
int proc_alloc(const char *, proc_t **);
