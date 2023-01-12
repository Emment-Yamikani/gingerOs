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

int pgroup_free(PGROUP pgroup)
{
    pgroup_assert(pgroup);
    if (pgroup->pg_procs)
    {
        queue_lock(pgroup->pg_procs);
        queue_free(pgroup->pg_procs);
    }

    assert(0 <= (int)atomic_read(&pgroup->pg_refs), "error pg->refs not \'<= 0\'");

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

    if ((pgroup = kmcreate(sizeof *pgroup)) == NULL)
    {
        err = -ENOMEM;
        goto error;
    }

    memset(pgroup, 0, sizeof *pgroup);

    pgroup->pg_pid = pgid;
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

    proc->pgroup = pgroup;

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
    return pgroup_remove(pgroup, proc);
}

int session_free(SESSION ss)
{
    session_assert(ss);

    if (ss->ss_pgroups)
    {
        queue_lock(ss->ss_pgroups);
        queue_free(ss->ss_pgroups);
    }

    assert(0 <= (int)atomic_read(&ss->ss_refs), "error ss->refs not \'<= 0\'");

    if (ss->ss_lock)
        spinlock_free(ss->ss_lock);

    kfree(ss);

    return 0;
}

int session_create(proc_t *leader, SESSION *ref)
{
    int err = 0;
    pid_t sid = 0;
    SESSION ss = NULL;
    char *name = NULL;
    PGROUP pgroup = NULL;
    spinlock_t *lock = NULL;
    queue_t *ss_queue = NULL;

    assert(ref, "no session ref");

    proc_lock(leader);

    sid = leader->pid;

    if ((name = strcat_num("session-", sid, 10)))
    {
        err = -ENOMEM;
        proc_unlock(leader);
        goto error;
    }

    if ((err = spinlock_init(NULL, name, &lock)))
    {
        proc_unlock(leader);
        goto error;
    }

    if ((err = queue_new(name, &ss_queue)))
    {
        proc_unlock(leader);
        goto error;
    }

    if ((ss = kmalloc(sizeof *ss)) == NULL)
    {
        err = -ENOMEM;
        proc_unlock(leader);
        goto error;
    }

    memset(ss, 0, sizeof *ss);

    ss->ss_sid = sid;
    ss->ss_lock = lock;
    ss->ss_leader = leader;
    ss->ss_pgroups = ss_queue;
    atomic_incr(&ss->ss_refs);

    session_lock(ss);
    if ((err = session_create_pgroup(ss, &pgroup, leader)))
    {
        proc_unlock(leader);
        session_unlock(ss);
        goto error;
    }
    session_unlock(ss);

    if (name)
        kfree(name);

    *ref = ss;

    return 0;
error:
    if (ss)
        kfree(ss);
    if (name)
        kfree(name);
    if (lock)
        spinlock_free(lock);
    if (ss_queue)
    {
        queue_lock(ss_queue);
        queue_free(ss_queue);
    }
    klog(KLOG_FAIL, "failed to create session, error=%d\n", err);
    return err;
}

int session_create_pgroup(SESSION ss, proc_t *leader, PGROUP *ref)
{
    int err = 0;
    PGROUP pg = NULL;

    session_assert_lock(ss);
    proc_assert_lock(leader);
    assert(ref, "no pgroup ref");

    if ((err = pgroup_create(leader, &pg)))
        goto error;
        
    if ((err = session_add(ss, pg)))
        goto error;

    return 0;
error:
    if (pg)
        pgroup_free(pg);
    klog(KLOG_FAIL, "failed to create pgroup in session, error=%d\n", err);
    return err;
}

int session_add(SESSION ss, PGROUP pg)
{
    int err = 0;
    session_assert_lock(ss);
    pgroup_assert_lock(pg);

    if (session_contains(ss, pg))
    {
        err = -EEXIST;
        goto error;
    }

    queue_lock(ss->ss_pgroups);
    if (enqueue(ss->ss_pgroups, pg) == NULL)
    {
        err = -ENOMEM;
        queue_unlock(ss->ss_pgroups);
        goto error;
    }
    queue_unlock(ss->ss_pgroups);

    pg->pg_sessoin = ss;
    pgroup_incr(pg);

    return 0;
error:
    klog(KLOG_FAIL, "failed add pgroup to session, error=%d\n", err);
    return err;
}

int session_contains(SESSION ss, PGROUP pg)
{
    session_assert_lock(ss);
    pgroup_assert(pg);

    queue_lock(ss->ss_pgroups);
    forlinked(node, ss->ss_pgroups->head, node)
    {
        if (pg == (PGROUP)node->data)
        {
            queue_unlock(ss->ss_pgroups);
            return 1;
        }
    }
    queue_unlock(ss->ss_pgroups);
    return 0;
}

int session_lookup(SESSION ss, pid_t pgid, PGROUP *ref)
{
    PGROUP pg = NULL;
    session_assert_lock(ss);
    assert(ref, "no pgroup ref");

    queue_lock(ss->ss_pgroups);
    forlinked(node, ss->ss_pgroups->head, node->next)
    {
        pg = node->data;
        pgroup_lock(pg)
        if (pg->pg_pid == pgid)
        {
            *ref = pg;
            pgroup_unlock(pg);
            return 0;
        }
        pgroup_unlock(pg);
    }
    queue_unlock(ss->ss_pgroups);

    return -ENOENT;
}

int session_remove(SESSION ss, PGROUP pg)
{
    int err = 0;
    session_assert_lock(ss);
    pgroup_assert_lock(pg);

    if (session_contains(ss, pg) == 0)
    {
        err = -ENOENT;
        goto error;
    }

    queue_lock(ss->ss_pgroups);
    if ((err = queue_remove(ss->ss_pgroups, pg)))
    {
        queue_unlock(ss->ss_pgroups);
        goto error;
    }
    queue_unlock(ss->ss_pgroups);

    pg->pg_sessoin = NULL;

    return 0;
error:
    klog(KLOG_FAIL, "failed to remove pgroup from session, error=%d\n", err);
    return err;
}

int session_leave(SESSION ss, proc_t *p)
{
    int err = 0;
    PGROUP pg = NULL;

    proc_lock(p);
    session_lock(ss);
    pg = p->pgroup;

    if (session_contains(ss, pg) == 0)
    {
        err = -ENOENT;
        session_unlock(ss);
        proc_unlock(p);
        goto error;
    }

    pgroup_lock(pg);

    if ((err = pgroup_remove(pg, p)))
    {
        pgroup_unlock(pg);
        session_unlock(ss);
        proc_unlock(p);
        goto error;
    }

    queue_lock(pg->pg_procs);

    if (queue_count(pg->pg_procs) == 0)
    {
        if ((err = session_remove(ss, pg)))
        {
            pgroup_unlock(pg);
            session_unlock(ss);
            proc_unlock(p);
            goto error;
        }

        pgroup_unlock(pg);
        queue_unlock(pg->pg_procs);
        
        pgroup_free(pg);

        session_unlock(ss);
        proc_unlock(p);
        return 0;
    }

    queue_unlock(pg->pg_procs);
    pgroup_unlock(pg);
    
    session_unlock(ss);
    proc_unlock(p);
    return 0;
error:
    klog(KLOG_FAIL, "failed to leave session, error=%d\n", err);
    return err;
}