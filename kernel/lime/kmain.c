#include <printk.h>
#include <arch/system.h>
#include <sys/kthread.h>
#include <locks/cond.h>
#include <fs/sysfile.h>
#include <sys/fcntl.h>
#include <lib/string.h>
#include <mm/kalloc.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <sys/sleep.h>
#include <sys/proc.h>
#include <fs/fs.h>

void *kthread_main(void *arg)
{
    (void)arg;
    klog(KLOG_OK, "\e[0;011m\'kernel main thread\'\e[0m running\n");

    proc_init("/init");

    klog(KLOG_OK, "kthread_main done executing\n");
    return NULL;
}