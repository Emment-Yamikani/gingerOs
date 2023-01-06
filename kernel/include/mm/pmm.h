#ifndef PMM_H
#define PMM_H 1

#include <lib/stdint.h>
#include <lib/stddef.h>
#include <locks/atomic.h>
#include <locks/spinlock.h>

#define MAXFRAMES 0x100000


extern spinlock_t *frameslk;
extern struct pm_frame_ { int refs; } frames[MAXFRAMES];

#define frames_lock() spin_lock(frameslk)
#define frames_unlock() spin_unlock(frameslk)
long frames_incr(int frame);
long frames_decr(int frame);
long frames_get_refs(int frame);

// physical memory manager entries
struct pmman
{
    int (*init)(void);    // initialize the physical memory manager
    void (*free)(uintptr_t);   // free a 4K page
    uintptr_t (*alloc)(void);   // allocate a 4K page
    size_t (*mem_used)(void); // used space (in KBs)
    size_t (*mem_free)(void); // free space (in KBs)
};

extern struct pmman pmman;

#endif //PMM_H