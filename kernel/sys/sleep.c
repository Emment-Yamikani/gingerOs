#include <printk.h>
#include <sys/sleep.h>
#include <dev/rtc.h>
#include <bits/errno.h>
#include <lime/jiffies.h>

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