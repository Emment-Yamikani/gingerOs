#ifndef GINGER_LOCKING_BARRIER_H
#define GINGER_LOCKING_BARRIER_H 1

#define barrier()\
    asm volatile ("":::"memory")

#ifndef CUP_RELAX
#define CPU_RELAX() asm volatile("pause" :: \
                                     : "memory")
#endif

#endif // GINGER_LOCKING_BARRIER_H