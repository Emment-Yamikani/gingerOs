#ifndef GINGER_LOCKING_BARRIER_H
#define GINGER_LOCKING_BARRIER_H 1

#define barrier()\
    asm volatile ("":::"memory")

#endif // GINGER_LOCKING_BARRIER_H