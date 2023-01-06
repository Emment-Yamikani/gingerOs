#ifndef MMAN_H
#define MMAN_H 1

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    extern int getpagesize(void);
    extern void *mmap(void *__addr, size_t __len, int __prot,
                      int __flags, int __fd, long __offset);

#ifdef __cplusplus
}
#endif

#endif // MMAN_H