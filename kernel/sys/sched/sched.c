#include <printk.h>
#include <sys/sched.h>
#include <arch/system.h>
#include <sys/proc.h>
#include <arch/i386/cpu.h>
#include <bits/errno.h>
#include <mm/kalloc.h>
#include <lime/string.h>
#include <lib/string.h>
#include <ds/queue.h>
#include <sys/sched.h>
#include <sys/thread.h>
#include <arch/sys/proc.h>
#include <arch/i386/paging.h>
#include <arch/sys/signal.h>

int sched_init(void)
{
    int err = 0;
    sched_queue_t *sq = NULL;
    queue_t *q = NULL;

    if (!(sq = kmalloc(sizeof *sq)))
        return -ENOMEM;

    memset(sq, 0, sizeof *sq);

    for (int i = 0; i < NLEVELS; ++i)
    {
        if ((err = queue_new(strcat_num("sched_queue", cpu->cpuid, 10), &q)))
        {
            panic("error initailizing scheduler queues, error=%d\n", err);
        }

        sq->level[i].queue = q;

        switch (i)
        {
        case 0:
            sq->level[i].quatum = 50;
            break;
        case 1:
            sq->level[i].quatum = 75;
            break;
        case 2:
            sq->level[i].quatum = 100;
            break;
        case 3:
            sq->level[i].quatum = 125;
            break;
        case 4:
            sq->level[i].quatum = 150;
            break;
        case 5:
            sq->level[i].quatum = 175;
            break;
        case 6:
            sq->level[i].quatum = 200;
            break;
        case 7:
            sq->level[i].quatum = 250;
            break;
        }
    }

    proc = NULL;
    current = NULL;
    cpu->ncli = 0;
    cpu->intena = 0;
    ready_queue = sq;
    return 0;
}

void sched(void)
{
    pushcli();
    int ncli = cpu->ncli;
    int intena = cpu->intena;
    
    //printk("%s:%d: %s() tid(%d), ncli: %d, intena: %d [%p]\n", __FILE__, __LINE__, __func__, current->t_tid, cpu->ncli, cpu->intena, return_address(0));
    
    current_assert_lock();
    swtch(&current->t_tarch->context, cpu->context);
    current_assert_lock();
    
    //printk("%s:%d: %s() tid(%d), ncli: %d, intena: %d [%p]\n", __FILE__, __LINE__, __func__, current->t_tid, cpu->ncli, cpu->intena, return_address(0));
    
    cpu->intena = intena;
    cpu->ncli = ncli;
    popcli();
}

int sched_setattr(thread_t *thread, int affinity, int core)
{
    assert(thread, "no thread");
    thread_assert_lock(thread);

    if ((affinity < SCHED_SOFT_AFFINITY) || (affinity > SCHED_HARD_AFFINITY))
        return -EINVAL;
    if ((core < 0) || (core > (int)(atomic_read(&cpus_online) - 1)))
        return -EINVAL;
    atomic_write(&thread->t_sched_attr.affinity, affinity);
    thread->t_sched_attr.core = &cpus[core];
    return 0;
}

void sched_set_priority(thread_t *thread, int priority)
{
    assert(thread, "no thread");
    thread_assert_lock(thread);
    if (priority < 0 || priority > 255)
        priority = SCHED_LOWEST_PRIORITY;
    atomic_write(&thread->t_sched_attr.t_priority, priority);
}

void sched_yield(void)
{
    current_assert();
    current_lock();
    current->t_state = T_READY;
    sched();
    current_unlock();
}

__noreturn void schedule(void)
{
    mmap_t *mmap = NULL;
    uintptr_t oldpgdir = 0;
    thread_t *thread = NULL;

    sched_init();
    for (;;)
    {
        mmap = NULL;
        proc = NULL;
        current = NULL;

        oldpgdir = 0;
        cpu->ncli = 0;
        cpu->intena = 0;

        sti();
        if (!(thread = sched_next()))
            continue;

        cli();

        if (thread_iskilled(thread))
        {
            thread->t_state = T_ZOMBIE;
            thread->t_exit = -ERFKILL;
            sched_zombie(thread);
            thread_unlock(thread);
            continue;
        }

        current = thread;

        mmap = current->mmap;
        proc = current->owner;

        if (mmap && current)
        {
            mmap_lock(mmap);
            oldpgdir = paging_switch(mmap->pgdir);
            mmap_unlock(mmap);
            thread_setkstack(current->t_tarch);
        }

        fpu_disable();

        current_assert_lock();
        swtch(&cpu->context, current->t_tarch->context);
        current_assert_lock();

        paging_switch(oldpgdir);

        switch (current->t_state)
        {
        case T_EMBRYO:
            panic("embryo was allowed to running\n");
            break;
        case T_ZOMBIE:
            assert(!sched_zombie(current), "couldn't zombie");
            fpu_thread = NULL;
            current_unlock();
            break;
        case T_READY:
            sched_park(current);
            current_unlock();
            break;
        case T_RUNNING:
            panic("thread not running\n");
            break;
        case T_ISLEEP:
            current_unlock();
            break;
        default:
            panic("tid: %d state: %d\n", current->t_tid, current->t_state);
        }
    }
}