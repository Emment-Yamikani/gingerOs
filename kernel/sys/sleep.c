#include <printk.h>
#include <sys/sleep.h>
#include <dev/rtc.h>
#include <bits/errno.h>

long sleep(long sec)
{
    while (sec) {if (rtc_wait()) break; sec--;}
    return sec;
}