#include <lime/jiffies.h>

static jiffies64_t jiffies64 = 0;

static spinlock_t *jiffies_lock = SPINLOCK_NEW("jiffies_lock");

void jiffies_update(void)
{
    spin_lock(jiffies_lock);
    jiffies64++;
    spin_unlock(jiffies_lock);
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
