#include <printk.h>
#include <arch/system.h>
#include <sys/kthread.h>
#include <locks/cond.h>
#include <fs/sysfile.h>
#include <dev/fb.h>
#include <sys/fcntl.h>
#include <lib/string.h>
#include <mm/kalloc.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/mmap.h>
#include <sys/sleep.h>
#include <sys/proc.h>
#include <fs/fs.h>
#include <lime/jiffies.h>

void *kthread_main(void *arg __unused)
{
    (void)arg;
    klog(KLOG_OK, "\e[0;011m\'kernel main thread\'\e[0m running\n");

    tid_t tid = 0;
    int tcount = 0;
    int nthreads = 0;
    void *retval = NULL;
    thread_t **tv = NULL;
    thread_t *thread = NULL;

    start_builtin_threads(&nthreads, &tv);
    tcount = nthreads;

    printk("%d builtin threads started\n", nthreads);

    proc_init("/init");

    loop()
    {
        for (int i = 0; i < nthreads; ++i)
        {
            if ((thread = tv[i]))
            {
                thread_lock(thread);
                tid = thread->t_tid;
                if (__thread_zombie(thread) || __thread_terminated(thread))
                {
                    if (thread_wait(thread, 1, &retval) == 0)
                    {
                        printk("%s:%d: thread: %d: exited, status: %p\n", __FILE__, __LINE__, tid, retval);
                        tcount--;
                        tv[i] = NULL;
                        continue;
                    }
                }
                thread_unlock(thread);
            }
        }
        if (tcount <= 0)
            break;
        sleep(4);
    }

    klog(KLOG_OK, "kthread_main done executing\n");
    return NULL;
}