#include <fs/posix.h>
#include <sys/system.h>
//#include <bits/fcntl.h>
//#include <fs/stat.h>
//#include <sys/sched.h>
#include <printk.h>

int posix_file_open(struct file *file __unused, int mode __unused, ...)
{
    return 0;
}
