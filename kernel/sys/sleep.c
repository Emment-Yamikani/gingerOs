#include <printk.h>
#include <sys/sleep.h>
#include <dev/rtc.h>
#include <bits/errno.h>
#include <lime/jiffies.h>
#include <ds/queue.h>
#include <sys/sched.h>
#include <arch/i386/cpu.h>
#include <sys/thread.h>

static queue_t *park_queue = QUEUE_NEW("park_queue");

long sleep(long sec)
{
    while (sec) {if (rtc_wait()) break; sec--;}
    return sec;
}

long wait_ms(long ms)
{
    jiffies_t now = jiffies_get() + ms;
    while (time_before(jiffies_get(), now));
    return ms;
}

int park(void)
{
    int err = 0;
    current_lock();
    err = xched_sleep(park_queue, NULL);
    current_unlock();
    return err;
}

int unpark(tid_t tid)
{
    int err = 0;
    thread_t *thread = NULL;

    queue_lock(park_queue);
    if ((err = queue_get_thread(park_queue, tid, &thread)))
        goto error;

    if (thread == current) {
        err = -EINVAL;
        thread_unlock(thread);
        goto error;
    }

    if ((err = thread_wake_n(thread))) {
        thread_unlock(thread);
        goto error;
    }
    
    thread_unlock(thread);
    queue_unlock(park_queue);
    return 0;
error:
    queue_unlock(park_queue);
    return err;
}