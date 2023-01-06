#pragma once

#include <locks/atomic.h>
#include <lib/stdint.h>
#include <lib/types.h>

#define NLEVELS 8
#define SCHED_HIGHEST_PRIORITY  0
#define SCHED_LOWEST_PRIORITY   255
#define SCHED_LEVEL(p) ((p)/((SCHED_LOWEST_PRIORITY + 1)/NLEVELS))

typedef struct level
{
    atomic_t pull;  //pull flag
    long     quatum;//quatum
    queue_t *queue; //queue
} level_t;

typedef struct sched_queue
{
    level_t level[NLEVELS];
} sched_queue_t;

/*queue up a thread*/
int sched_park(thread_t *);

/**
 * @brief give up the cpu for on scheduling round
 * 
 */
void sched_yield(void);

/**
 * @brief put thread on the zombie queue.
 * then signal all threads waiting for a signal from this thread.
 * caller must hold thread->t_lock.
 *
 * @param thread
 * @return int
 */
int sched_zombie(thread_t *thread);

/**
 * @brief wake one 'thread' from 'sleep_queue'.
 * 
 * @param sleep_queue 
 */
void sched_wake1(queue_t *sleep_queue);

/**
 * @brief causes the current 'thread' to sleep on 'sleep_queue'.
 * Also releases 'lock' if specified
 * and switches to the scheduler while holding current->t_lock
 * @param sleep_queue 
 * @param lock 
 * @return int 
 */
int sched_sleep(queue_t *sleep_queue, spinlock_t *lock);

/**
 * @brief wake all threads on 'sleep-queue'
 * 
 * @param sleep_queue 
 * @return int the number of threads woken
 */
int sched_wakeall(queue_t *sleep_queue);

/*get a thread from a queue*/
thread_t *sched_next(void);

int sched_setattr(thread_t *thread, int affinity, int core);

void sched_set_priority(thread_t *thread, int priority);

int sched_init(void);

/*context switch back to the scheduler*/
extern void sched(void);

/*start running the scheduler*/
extern void schedule(void);