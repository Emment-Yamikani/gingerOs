#ifndef MMAN_H
#define MMAN_H 1

#include <stddef.h>

#define PROT_NONE           0x0 // Deny any access, do not ack accesses to this region.
#define PROT_READ           0x1 // The region is mapped for reading, must always be provided in "prot" argument.
#define PROT_WRITE          0x2 // The region is mapped for writing.
#define PROT_EXEC           0x4 // The paged in this region can be executed.

#define MAP_PRIVATE     0x0001
#define MAP_SHARED      0x0002
#define MAP_DONTEXPAND  0x0004
#define MAP_ANON        0x0008
#define MAP_ZERO        0x0010
#define MAP_MAPIN       0x0020
#define MAP_GROWSDOWN   0x0100
/*region is a stack*/
#define MAP_STACK       (MAP_GROWSDOWN)

/**
 * Map address range as given, 
 * and unmap any overlaping regions previously mapped.
*/
#define MAP_FIXED    0x1000
#define MAP_NORESERVE 

#ifdef __cplusplus
extern "C"
{
#endif

    extern int getpagesize(void);
    extern void *mmap(void *__addr, size_t __len, int __prot,
                      int __flags, int __fd, long __offset);
    
    extern int munmap(void *addr, size_t length);
    extern int mprotect(void *addr, size_t len, int prot);
#ifdef __cplusplus
}
#endif

#endif // MMAN_H