#include <ds/queue.h>
#include <lime/jiffies.h>
#include <sys/sched.h>
#include <sys/thread.h>


static jiffies64_t jiffies64 = 0;
static queue_t *jiffy_queue = QUEUE_NEW("jiffy_queue");
static spinlock_t *jiffies_lock = SPINLOCK_NEW("jiffies_lock");

void jiffies_update(void)
{
    spin_lock(jiffies_lock);
    jiffies64++;
    spin_unlock(jiffies_lock);
    xched_wake1(jiffy_queue);
}

jiffies_t jiffies_get(void)
{
    jiffies_t val = 0;
    spin_lock(jiffies_lock);
    val = jiffies64;
    spin_unlock(jiffies_lock);
    return val;
}

jiffies64_t jiffies64_get(void)
{
    jiffies64_t val = 0;
    spin_lock(jiffies_lock);
    val = jiffies64;
    spin_unlock(jiffies_lock);
    return val;
}

int jiffies_sleep(jiffies_t time)
{
    int err = 0;
    time += jiffies_get();
    while (time_before(jiffies_get(), time)) {
        current_lock();
        if ((err = xched_sleep(jiffy_queue, NULL))) {
            current_unlock();
            return err;
        }
        current_unlock();
    }
    return err;
}