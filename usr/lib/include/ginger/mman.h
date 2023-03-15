#ifndef MMAN_H
#define MMAN_H 1

#include <stddef.h>

#define PROT_NONE           0x0 // Deny any access, do not ack accesses to this region.
#define PROT_READ           0x1 // The region is mapped for reading, must always be provided in "prot" argument.
#define PROT_WRITE          0x2 // The region is mapped for writing.
#define PROT_EXEC           0x4 // The paged in this region can be executed.

#define MAP_PRIVATE         0x1     // Only calling process has access to this region.
#define MAP_SHARED          0x2     // May be shared with other processes other thn the calling process.
#define MAP_FIXED           0x4     // Fixed mapping constraints.
#define MAP_FIXED_NOREPLCE  0x8     // Similar to MAP_FIXED.
#define MAP_32BIT           0x10    // Map the region into the lower 2GiB.
#define MAP_ANON            0x20    // The mapping is not backed by a file and fd is ignored.
#define MAP_ANONYMOUS       MAP_ANON
#define MAP_GROWSDOWN       0x40    // This region grouws downwards.
#define MAP_LOCKED          0x80    // Lock this region as if with "mlock()".
#define MAP_HUGE            0x100   // Use Huge pages for this mapping.
#define MAP_STACK           0x200   // Not defined yet.
#define MAP_UNINITIALIZED   0x400   // Do not initialize the contents of this region.
#define MAP_SHARED_VALIDATE 0x800   // Similar to MAP_SHARED but validates "flags"
#define MAP_NORESERVE 

#ifdef __cplusplus
extern "C"
{
#endif

    extern int getpagesize(void);
    extern void *mmap(void *__addr, size_t __len, int __prot,
                      int __flags, int __fd, long __offset);
    
    extern int munmap(void *addr, size_t length);
#ifdef __cplusplus
}
#endif

#endif // MMAN_H