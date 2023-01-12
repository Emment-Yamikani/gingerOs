#pragma once

#include <locks/spinlock.h>
#include <locks/atomic.h>
#include <lime/assert.h>
#include <ds/queue.h>
#include <sys/proc.h>

struct pgroup;
struct session;

typedef struct pgroup
{
    pid_t pg_pid; /*process group ID*/
    proc_t *pg_leader; /*process group leader*/
    queue_t *pg_procs; /*process queue*/
    atomic_t pg_refs; /*process group reference count*/
    struct session *pg_sessoin; /*session to which this process group belongs*/
    spinlock_t *pg_lock; /*process group struct lock*/
}pgroup_t, *PGROUP;

/*assert PGROUP*/
#define pgroup_assert(pg) assert(pg, "no pgroup")

/*assert PGROUP PTR and PGROUP lock*/
#define pgroup_assert_lock(pg) {pgroup_assert(pg); spin_assert_lock(pg->pg_lock);}

/*assert and lock PGROUP struct*/
#define pgroup_lock(pg) {pgroup_assert(pg); spin_lock(pg->pg_lock);}

/*assert and unlock PGROUP struct*/
#define pgroup_unlock(pg) {pgroup_assert(pg); spin_unlock(pg->pg_lock);}

/*increment PGROUP reference count*/
#define pgroup_incr(pg) {pgroup_assert(pg); atomic_incr(&pg->pg_refs);}

/*decrement PGROUP reference count*/
#define pgroup_decr(pg) {pgroup_assert(pg); atomic_decr(&pg->pg_refs);}

/*determine whether is session leader*/
static inline int ispgroup_leader(PGROUP pg, proc_t *p) {pgroup_assert_lock(pg); proc_assert(p); return pg->pg_leader == p;}

int pgroup_free(PGROUP);
int pgroup_leave(PGROUP);
int pgroup_add(PGROUP, proc_t *);
int pgroup_remove(PGROUP, proc_t *);
int pgroup_contains(PGROUP, proc_t *);
int pgroup_create(proc_t *, pgroup_t **);
int pgroup_lookup(PGROUP, pid_t, proc_t **);

typedef struct session
{
    pid_t ss_sid; /*session ID*/
    proc_t *ss_leader; /*session leader*/
    queue_t *ss_pgroups; /*session queue*/
    atomic_t ss_refs; /*session reference count*/
    spinlock_t *ss_lock; /*session struct lock*/
}session_t, *SESSION;

/*assert SESSION*/
#define session_assert(ss) assert(ss, "no session")

/*assert SESSION PTR and SESSION lock*/
#define session_assert_lock(ss) {session_assert(ss); spin_assert_lock(ss->ss_lock);}

/*assert and lock SESSION struct*/
#define session_lock(ss) {session_assert(ss); spin_lock(ss->ss_lock);}

/*assert and unlock SESSION struct*/
#define session_unlock(ss) {session_assert(ss); spin_unlock(ss->ss_lock);}

/*increment SESSION reference count*/
#define session_incr(ss) {session_assert(ss); atomic_incr(&ss->ss_refs);}

/*decrement SESSION reference count*/
#define session_decr(ss) {session_assert(ss); atomic_decr(&ss->ss_refs);}

/*determine whether is session leader*/
static inline int issession_leader(SESSION ss, proc_t *p) {session_assert_lock(ss); proc_assert(p); return ss->ss_leader == p;}

int session_free(SESSION);
int session_leave(SESSION, proc_t *);
int session_add(SESSION, PGROUP);
int session_remove(SESSION, PGROUP);
int session_contains(SESSION, PGROUP);
int session_alloc(proc_t *, session_t **);
int session_lookup(SESSION, pid_t, pgroup_t **);
int session_create_pgroup(SESSION, proc_t *, PGROUP *);