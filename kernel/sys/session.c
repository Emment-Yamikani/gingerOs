#include <printk.h>
#include <sys/proc.h>
#include <sys/thread.h>
#include <arch/i386/cpu.h>
#include <mm/kalloc.h>
#include <lib/string.h>
#include <lime/string.h>
#include <sys/session.h>
#include <sys/sched.h>
#include <bits/errno.h>

static queue_t *sessions = QUEUE_NEW("All-sessions");


int pgroup_free(PGROUP pgroup)
{
    pgroup_assert(pgroup);
    if (pgroup->pg_procs)
    {
        queue_lock(pgroup->pg_procs);
        queue_free(pgroup->pg_procs);
    }

    assert(0 <= (int)atomic_read(&pgroup->pg_refs), "error pgroup->refs not \'<= 0\'");

    if (pgroup->pg_lock)
        spinlock_free(pgroup->pg_lock);

    kfree(pgroup);

    return 0;
}

int pgroup_create(proc_t *leader, PGROUP *pgref)
{
    int err = 0;
    pid_t pgid = 0;
    char *name = NULL;
    pgroup_t *pgroup = NULL;
    spinlock_t *lock = NULL;
    queue_t *pgroup_queue = NULL;

    proc_assert_lock(leader);
    assert(pgref, "no pgroup ref ptr");

    pgid = leader->pid;

    if ((name = strcat_num("pgroup", pgid, 10)) == NULL)
    {
        err = -ENOMEM;
        goto error;
    }

    if ((err = spinlock_init(NULL, name, &lock)))
        goto error;

    if ((err = queue_new(name, &pgroup_queue)))
        goto error;

    if ((pgroup = kmalloc(sizeof *pgroup)) == NULL)
    {
        err = -ENOMEM;
        goto error;
    }

    memset(pgroup, 0, sizeof *pgroup);

    pgroup->pg_id = pgid;
    pgroup->pg_lock = lock;
    pgroup->pg_leader = leader;
    pgroup->pg_procs = pgroup_queue;
    atomic_incr(&pgroup->pg_refs);

    pgroup_lock(pgroup);

    if ((err = pgroup_add(pgroup, leader)))
    {
        pgroup_unlock(pgroup);
        goto error;
    }

    pgroup_unlock(pgroup);

    if (name)
        kfree(name);

    *pgref = pgroup;
    return 0;
error:
    if (pgroup_queue)
    {
        queue_lock(pgroup_queue);
        queue_free(pgroup_queue);
    }
    if (lock)
        spinlock_free(lock);
    if (pgroup)
        kfree(pgroup);
    if (name)
        kfree(name);
    klog(KLOG_FAIL, "failed to create pgroup\n");
    return err;
}

int pgroup_add(PGROUP pgroup, proc_t *p)
{
    int err = 0;
    proc_assert_lock(p);
    pgroup_assert_lock(pgroup);

    if (pgroup_contains(pgroup, p))
    {
        err = -EEXIST;
        goto error;
    }

    queue_lock(pgroup->pg_procs);
    if ((enqueue(pgroup->pg_procs, p)) == NULL)
    {
        err = -ENOMEM;
        queue_unlock(pgroup->pg_procs);
        goto error;
    }
    queue_unlock(pgroup->pg_procs);

    p->pgroup = pgroup;

    return 0;
error:
    klog(KLOG_FAIL, "failed to add proc to pgroup, error=%d\n", err);
    return err;
}

int pgroup_lookup(PGROUP pgroup, pid_t pid, proc_t **ref)
{
    proc_t *p = NULL;
    assert(ref, "no proc ref");
    pgroup_assert_lock(pgroup);

    queue_lock(pgroup->pg_procs);
    forlinked(node, pgroup->pg_procs->head, node->next)
    {
        p = node->data;
        proc_lock(p);
        if (p->pid == pid)
        {
            *ref = p;
            proc_unlock(p);
            queue_unlock(pgroup->pg_procs);
            return 0;
        }
        proc_unlock(p);
    }
    queue_unlock(pgroup->pg_procs);
    return -ENOENT;
}

int pgroup_contains(PGROUP pgroup, proc_t *p)
{
    proc_assert(p);
    pgroup_assert_lock(pgroup);

    queue_lock(pgroup->pg_procs);
    forlinked(node, pgroup->pg_procs->head, node->next)
    {
        if (p == (proc_t *)node->data)
        {
            queue_unlock(pgroup->pg_procs);
            return 1;
        }
    }
    queue_unlock(pgroup->pg_procs);
    return 0;
}

int pgroup_remove(PGROUP pgroup, proc_t *p)
{
    int err = 0;
    proc_assert_lock(p);
    pgroup_assert_lock(pgroup);

    if (pgroup_contains(pgroup, p) == 0)
    {
        err = -ENOENT;
        goto error;
    }

    queue_lock(pgroup->pg_procs);
    if ((err = queue_remove(pgroup->pg_procs, p)))
    {
        queue_unlock(pgroup->pg_procs);
        goto error;
    }
    queue_unlock(pgroup->pg_procs);

    p->pgroup = NULL;

    return 0;
error:
    klog(KLOG_FAIL, "failed to remove proc from pgroup, error=%d\n", err);
    return err;
}

int pgroup_leave(PGROUP pgroup)
{
    pgroup_lock(pgroup);
    int err = pgroup_remove(pgroup, proc);
    pgroup_unlock(pgroup);
    return err;
}

int session_free(SESSION session)
{
    session_assert(session);

    queue_lock(sessions);
    queue_remove(sessions, session);
    queue_unlock(sessions);

    if (session->ss_pgroups)
    {
        queue_lock(session->ss_pgroups);
        queue_free(session->ss_pgroups);
    }

    assert(0 <= (int)atomic_read(&session->ss_refs), "error session->refs not \'<= 0\'");

    if (session->ss_lock)
        spinlock_free(session->ss_lock);

    kfree(session);

    return 0;
}

int session_create(proc_t *leader, SESSION *ref)
{
    int err = 0;
    pid_t sid = 0;
    SESSION session = NULL;
    char *name = NULL;
    PGROUP pgroup = NULL;
    spinlock_t *lock = NULL;
    queue_t *ss_queue = NULL;

    proc_assert_lock(leader);
    assert(ref, "no session ref");

    sid = leader->pid;

    if ((name = strcat_num("session-", sid, 10)) == NULL)
    {
        err = -ENOMEM;
        goto error;
    }

    if ((err = spinlock_init(NULL, name, &lock)))
        goto error;

    if ((err = queue_new(name, &ss_queue)))
        goto error;

    if ((session = kmalloc(sizeof *session)) == NULL)
    {
        err = -ENOMEM;
        goto error;
    }

    memset(session, 0, sizeof *session);

    session->ss_sid = sid;
    session->ss_lock = lock;
    session->ss_leader = leader;
    session->ss_pgroups = ss_queue;
    atomic_incr(&session->ss_refs);

    session_lock(session);
    if ((err = session_create_pgroup(session, leader, &pgroup)))
    {
        session_unlock(session);
        goto error;
    }
    session_unlock(session);

    if (name)
        kfree(name);

    *ref = session;

    queue_lock(sessions);
    if ((enqueue(sessions, session)) == NULL)
    {
        queue_unlock(sessions);
        err = -ENOMEM;
        goto error;
    }
    queue_unlock(sessions);

    return 0;
error:
    if (session)
        kfree(session);
    if (name)
        kfree(name);
    if (lock)
        spinlock_free(lock);
    if (ss_queue)
    {
        queue_lock(ss_queue);
        queue_free(ss_queue);
    }
    if (pgroup)
        pgroup_free(pgroup);
    klog(KLOG_FAIL, "failed to create session, error=%d\n", err);
    return err;
}

int session_create_pgroup(SESSION session, proc_t *leader, PGROUP *ref)
{
    int err = 0;
    PGROUP pgroup = NULL;

    session_assert_lock(session);
    proc_assert_lock(leader);
    assert(ref, "no pgroup ref");

    if ((err = pgroup_create(leader, &pgroup)))
        goto error;
    
    pgroup_lock(pgroup);
    //printk("created PGID: %d\n", pgroup->pg_id);
    if ((err = session_add(session, pgroup)))
    {
        pgroup_unlock(pgroup);
        goto error;
    }
    pgroup_unlock(pgroup);

    leader->session = session;
    *ref = pgroup;
    return 0;
error:
    if (pgroup)
        pgroup_free(pgroup);
    klog(KLOG_FAIL, "failed to create pgroup in session, error=%d\n", err);
    return err;
}

int session_add(SESSION session, PGROUP pgroup)
{
    int err = 0;
    session_assert_lock(session);
    pgroup_assert_lock(pgroup);

    if (session_contains(session, pgroup))
    {
        err = -EEXIST;
        goto error;
    }

    queue_lock(session->ss_pgroups);
    if (enqueue(session->ss_pgroups, pgroup) == NULL)
    {
        err = -ENOMEM;
        queue_unlock(session->ss_pgroups);
        goto error;
    }
    queue_unlock(session->ss_pgroups);

    pgroup->pg_sessoin = session;
    
    pgroup_incr(pgroup);

    return 0;
error:
    klog(KLOG_FAIL, "failed add pgroup to session, error=%d\n", err);
    return err;
}

int session_contains(SESSION session, PGROUP pgroup)
{
    session_assert_lock(session);
    pgroup_assert(pgroup);

    queue_lock(session->ss_pgroups);
    forlinked(node, session->ss_pgroups->head, node->next)
    {
        if (pgroup == (PGROUP)node->data)
        {
            queue_unlock(session->ss_pgroups);
            return 1;
        }
    }
    queue_unlock(session->ss_pgroups);

    return 0;
}

int session_lookup(SESSION session, pid_t pgid, PGROUP *ref)
{
    PGROUP pgroup = NULL;
    session_assert_lock(session);
    assert(ref, "no pgroup ref");

    queue_lock(session->ss_pgroups);
    forlinked(node, session->ss_pgroups->head, node->next)
    {
        pgroup = node->data;
        pgroup_lock(pgroup)
        if (pgroup->pg_id == pgid)
        {
            *ref = pgroup;
            queue_unlock(session->ss_pgroups);
            return 0;
        }
        pgroup_unlock(pgroup);
    }
    queue_unlock(session->ss_pgroups);

    return -ENOENT;
}

int session_remove(SESSION session, PGROUP pgroup)
{
    int err = 0;
    session_assert_lock(session);
    pgroup_assert_lock(pgroup);

    if (session_contains(session, pgroup) == 0)
    {
        err = -ENOENT;
        goto error;
    }

    queue_lock(session->ss_pgroups);
    if ((err = queue_remove(session->ss_pgroups, pgroup)))
    {
        queue_unlock(session->ss_pgroups);
        goto error;
    }
    queue_unlock(session->ss_pgroups);

    pgroup->pg_sessoin = NULL;

    return 0;
error:
    klog(KLOG_FAIL, "failed to remove pgroup from session, error=%d\n", err);
    return err;
}

int session_leave(SESSION session, proc_t *p)
{
    int err = 0, count = 0;
    PGROUP pgroup = NULL;

    proc_assert_lock(p);
    session_assert_lock(session);
    pgroup = p->pgroup;

    if (session_contains(session, pgroup) == 0)
    {
        err = -ENOENT;
        goto error;
    }

    pgroup_lock(pgroup);

    if ((err = pgroup_remove(pgroup, p)))
    {
        pgroup_unlock(pgroup);
        goto error;
    }

    p->session = NULL;

    queue_lock(pgroup->pg_procs);
    if ((count = queue_count(pgroup->pg_procs)) == 0)
    {
        if ((err = session_remove(session, pgroup)))
        {
            pgroup_unlock(pgroup);
            goto error;
        }

        //klog(KLOG_WARN, "removed pgroup(%d) from session(%d)\n", pgroup->pg_id, session->ss_sid);
        queue_unlock(pgroup->pg_procs);
        pgroup_unlock(pgroup);
        pgroup_free(pgroup);
        return 0;
    }

    //printk("%d processes remaining in pgroup(%d)\n", count, pgroup->pg_id);
    queue_unlock(pgroup->pg_procs);
    pgroup_unlock(pgroup);
    return 0;
error:
    klog(KLOG_FAIL, "failed to leave session, error=%d\n", err);
    return err;
}

int session_pgroup_join(SESSION session, pid_t pgid, proc_t *process)
{
    int err = 0;
    PGROUP pgroup = NULL;

    if (pgid < 0) return -EINVAL;

    proc_assert_lock(proc);
    session_assert_lock(session);

    if ((err = session_lookup(session, pgid, &pgroup)))
        goto error;
    
    if (process->pgroup == pgroup) // Already in this pgroup
        goto done;

    // found the pgroup with PGID == pgid
    
    /*process shouldn't belong to any session or a pgroup*/
    if ((err = session_exit(session, process, 0)))
    {
        pgroup_unlock(pgroup);
        goto error;
    }

    if ((err = pgroup_add(pgroup, process)))
    {
        pgroup_unlock(pgroup);
        goto error;
    }
    
done:
    //klog(KLOG_OK, "process(%d) successfully joined pgroup(%d)\n", process->pid, pgroup->pg_id);
    pgroup_unlock(pgroup);
    process->session = session;
    return 0;
error:
    klog(KLOG_FAIL, "failed to join PGID(%d), error: %d\n", pgid, err);
    return err;
}

ssize_t session_pgroup_count(SESSION session)
{
    ssize_t count = 0;
    session_assert_lock(session);
    session_lock_queue(session);
    count = queue_count(session->ss_pgroups);
    session_unlock_queue(session);
    return count;
}

int session_exit(SESSION session, proc_t *process, int free_session)
{
    int err = 0, locked = 0;
    ssize_t count = 0;

    proc_assert_lock(process);

    if (spin_holding(session->ss_lock) == 0){
        session_lock(session);
        locked = 1;
    }

    // make sure process is in this session
    if (session != process->session)
    {
        err = -EINVAL;
        if (spin_holding(session->ss_lock) && locked)
            session_unlock(session);
        goto error;
    }

    // leave this session
    if ((err = session_leave(session, process))){
        if (spin_holding(session->ss_lock) && locked)
            session_unlock(session);
        goto error;
    }
    
    // If there are no more pgroups in this session free it
    count = session_pgroup_count(session);
    if ((count <= 0) && free_session){
        klog(KLOG_WARN, "freeing session(count: %d)\n", count);
        session_free(session);
        return 0;
    }

    //klog(KLOG_WARN, "session(count: %d), free: %d\n", count, free_session);

    if (spin_holding(session->ss_lock) && locked)
        session_unlock(session);
    return 0;
error:
    klog(KLOG_FAIL, "failed to exit session, error: %d\n", err);
    return err;
}

int sessions_get(pid_t sid, SESSION *ref)
{
    SESSION session = NULL;
    queue_lock(sessions);
    forlinked(node, sessions->head, node->next)
    {
        session = node->data;
        session_lock(session);
        if (session->ss_sid == sid)
        {
            if (ref)
            {
                *ref = session;
                queue_unlock(sessions);
                return 0;
            }
            session_unlock(session);
            queue_unlock(sessions);
            return 0;
        }
        session_unlock(session);
    }
    queue_unlock(sessions);
    return -ESRCH;
}

int sessions_find_pgroup(pid_t pgid, PGROUP *ref)
{
    PGROUP pgroup = NULL;
    SESSION session = NULL;

    queue_lock(sessions);
    forlinked(node, sessions->head, node->next)
    {
        session = node->data;
        session_lock(session);
        
        if (session_lookup(session, pgid, &pgroup) == 0)
        {
            if (ref)
            {
                *ref = pgroup;
                session_unlock(session);
                queue_unlock(sessions);
                return 0;
            }

            pgroup_unlock(pgroup);
            session_unlock(session);
            queue_unlock(sessions);
            return 0;
        }

        session_unlock(session);
    }
    queue_unlock(sessions);
    return -ESRCH;
}

int session_end(SESSION session);
int session_pgroup_remove(SESSION session);