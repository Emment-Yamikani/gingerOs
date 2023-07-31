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
#include <mm/mm_zone.h>

void *kthread_main(void *arg __unused)
{
    (void)arg;
    tid_t tid = 0;
    int tcount = 0;
    int nthreads = 0;
    void *retval = NULL;
    thread_t **tv = NULL;
    thread_t *thread = NULL;
    
    klog(KLOG_OK, "\e[0;011m\'kernel main thread\'\e[0m running\n");

    start_builtin_threads(&nthreads, &tv);
    tcount = nthreads;

    printk("%d builtin thread(s) started\n", nthreads);

    // loop();
    proc_init("/init");

    loop()
    {
        for (int i = 0; i < nthreads; ++i)
        {
            sleep(4);
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
    }

    klog(KLOG_OK, "kthread_main done executing\n");
    return NULL;
}

void *file_create(void *arg __unused) {
    int err = 0;
    int mode = 0;
    uio_t uio = {0};
    inode_t *inode = NULL;

    err = vfs_open("/tmp/file", &uio, O_RDWR | O_CREAT, mode, &inode);
    if (err) goto error;
    iwrite(inode, 0, "Hello, World\n", 14);

    iwrite(inode, 12, " Xkay you\n", 11);

    char buf[25] = {0};
    iread(inode, 0, buf, sizeof buf -1);
    
    printk("filesz: %d, data: %s\n", inode->i_size, buf);
    loop();
error:
    printk("failed to open file, error: %d\n", err);
    loop();
    return NULL;
}

BUILTIN_THREAD(file_creation, file_create, NULL);