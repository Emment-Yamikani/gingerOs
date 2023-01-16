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
    pid_t pg_id; /*process group ID*/
    proc_t *pg_leader; /*process group leader*/
    queue_t *pg_procs; /*process queue*/
    atomic_t pg_refs; /*process group reference count*/
    struct session *pg_sessoin; /*session to which this process group belongs*/
    spinlock_t *pg_lock; /*process group struct lock*/
}pgroup_t, *PGROUP;

/*assert PGROUP*/
#define pgroup_assert(pgroup) assert(pgroup, "no pgroup")

/*assert PGROUP PTR and PGROUP lock*/
#define pgroup_assert_lock(pgroup) {pgroup_assert(pgroup); spin_assert_lock(pgroup->pg_lock);}

/*assert and lock PGROUP struct*/
#define pgroup_lock(pgroup) {pgroup_assert(pgroup); spin_lock(pgroup->pg_lock);}

/*assert and unlock PGROUP struct*/
#define pgroup_unlock(pgroup) {pgroup_assert(pgroup); spin_unlock(pgroup->pg_lock);}

#define pgroup_lock_queue(pgroup) {pgroup_assert_lock(pgroup); queue_lock(pgroup->pg_procs);}
#define pgroup_unlock_queue(pgroup) {pgroup_assert_lock(pgroup); queue_unlock(pgroup->pg_procs);};

/*increment PGROUP reference count*/
#define pgroup_incr(pgroup) {pgroup_assert(pgroup); atomic_incr(&pgroup->pg_refs);}

/*decrement PGROUP reference count*/
#define pgroup_decr(pgroup) {pgroup_assert(pgroup); atomic_decr(&pgroup->pg_refs);}

/*determine whether is session leader*/
static inline int ispgroup_leader(PGROUP pgroup, proc_t *p) {pgroup_assert_lock(pgroup); proc_assert(p); return pgroup->pg_leader == p;}

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
#define session_assert(session) assert(session, "no session")

/*assert SESSION PTR and SESSION lock*/
#define session_assert_lock(session) {session_assert(session); spin_assert_lock(session->ss_lock);}

/*assert and lock SESSION struct*/
#define session_lock(session) {session_assert(session); spin_lock(session->ss_lock);}

/*assert and unlock SESSION struct*/
#define session_unlock(session) {session_assert(session); spin_unlock(session->ss_lock);}

/*increment SESSION reference count*/
#define session_incr(session) {session_assert(session); atomic_incr(&session->ss_refs);}

/*decrement SESSION reference count*/
#define session_decr(session) {session_assert(session); atomic_decr(&session->ss_refs);}

#define session_lock_queue(session) {session_assert_lock(session); queue_lock(session->ss_pgroups);}
#define session_unlock_queue(session) {session_assert_lock(session); queue_unlock(session->ss_pgroups);};

/*determine whether is session leader*/
static inline int issession_leader(SESSION session, proc_t *p) {session_assert_lock(session); proc_assert(p); return session->ss_leader == p;}

int session_free(SESSION);
int session_leave(SESSION, proc_t *);
int session_add(SESSION, PGROUP);
int session_remove(SESSION, PGROUP);
int session_contains(SESSION, PGROUP);
int session_create(proc_t *, SESSION *);
int session_lookup(SESSION, pid_t, pgroup_t **);
int session_create_pgroup(SESSION, proc_t *, PGROUP *);

ssize_t session_pgroup_count(SESSION session);
int session_exit(SESSION session, proc_t *process, int free_session);
int session_pgroup_join(SESSION session, pid_t pgid, proc_t *process);

int sessions_get(pid_t sid, SESSION *);
int sessions_find_pgroup(pid_t, PGROUP *);