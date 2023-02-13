#include <bits/errno.h>
#include <bits/dirent.h>
#include <fs/fs.h>
#include <mm/shm.h>
#include <lib/string.h>
#include <lime/string.h>
#include <lime/assert.h>
#include <printk.h>
#include <sys/mmap/mmap.h>
#include <sys/system.h>


void *mmap(void *addr __unused, size_t length __unused, int prot __unused, int flags __unused, int fd __unused, off_t offset __unused)
{
    return NULL;
}

int munmap(void *addr, size_t length);