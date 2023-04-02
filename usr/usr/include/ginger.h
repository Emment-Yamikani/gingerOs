#ifndef GINGER_H
#define GINGER_H 1

#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <thread.h>
#include <signal.h>
#include <ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <math.h>
#include <sys/fcntl.h>
#include <bits/dirent.h>
#include <bits/errno.h>

#include <ginger/mman.h>

#include <gfx/gfx.h>
#include <fbterm/fb.h>

#include <locking/atomic.h>
#include <locking/spinlock.h>
#include <locking/mutex.h>
#include <locking/cond.h>

extern void panic(const char *, ...);

#define foreach(element, list) \
    for (typeof(*list) *tmp = list, element = *tmp; element; element = *++tmp)

#define forlinked(element, list, iter) \
    for (typeof(list) element = list; element; element = iter)

#define _BS(b) ((1 << b))
#define _BITMASK(f, b) (f & (~b))
#define MIN(a, b) ((a < b) ? a : b)
#define SHL(a, b) (a << b)

#ifndef NELEM
#define NELEM(x) ((int)(sizeof(x) / sizeof(x[0])))
#endif // NELEM__ARRAY

#define __CAT(a, b) a##b

#define __fallthrough __attribute__((fallthrough))
#define __unused __attribute((unused))
#define __packed __attribute((packed))
#define __noreturn __attribute__((noreturn))
#define __align(n) __attribute((align(n)))
#define __section(s) __attribute__((section(s)))

#define __cast_to(a) (a)
#define __cast_to_type(t) (typeof(t))
#define __cast_void (void *)

#define CPU_RELAX() asm volatile("pause" :: \
                                     : "memory")
#define return_address(l) __builtin_return_address(l)

#endif // GINGER_H