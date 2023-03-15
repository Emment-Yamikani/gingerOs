#include <printk.h>
#include <ds/queue.h>
#include <sys/proc.h>
#include <sys/thread.h>
#include <sys/session.h>
#include <arch/i386/cpu.h>
#include <fs/sysfile.h>

int PROC_EXIT = 0;

void exit(int status)
{
    proc_t *child = NULL;
    proc_t *parent = NULL;
    SESSION session = NULL;
    queue_node_t *next = NULL;

    proc_assert(proc);

    if (proc == initproc)
        panic("init exiting\n");

    if (thread_kill_all() == -ERFKILL)
    {
        thread_exit(status);
    }

    proc_lock(proc);
    parent = proc->parent;
    session = proc->session;

    signals_cancel(proc);

    proc_lock(parent);

    queue_lock(processes);
    queue_remove(processes, proc);
    queue_unlock(processes);

    if (session)
        session_exit(session, proc, 1);

    for (int fd = 0; fd < NFILE; ++fd)
        close(fd);

    queue_lock(initproc->children);
    queue_lock(proc->children);

    forlinked(node, proc->children->head, next)
    {
        next = node->next;
        child = node->data;
        proc_lock(child);

        child->parent = initproc;
        queue_remove(proc->children, child);
        atomic_or(&child->flags, PROC_ORPHANED);
        enqueue(initproc->children, child);
        proc_unlock(child);
    }

    queue_unlock(proc->children);
    queue_unlock(initproc->children);

    mmap_lock(proc->mmap);
    mmap_clean(proc->mmap);
    mmap_unlock(proc->mmap);

    proc->state = ZOMBIE;
    proc->exit = status;

    cond_broadcast(proc->wait_child);
    cond_broadcast(parent->wait_child);

    printk("[\e[0;11m%d\e[0m:\e[0;012m%d\e[0m:\e[0;02m%d\e[0m]: '%s' "
           "\e[0;04mexit(\e[0;017m%d\e[0m)\e[0m\n",
           parent->pid, proc->pid, thread_self(), proc->name, status);
    proc_unlock(proc);
    proc_unlock(parent);

    thread_exit(status);
    panic("exit unsuccessful\n");
}
