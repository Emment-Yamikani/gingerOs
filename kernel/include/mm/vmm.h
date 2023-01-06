#ifndef VMM_H
#define VMM_H 1

#include <lib/types.h>
#include <lib/stddef.h>
#include <lib/stdint.h>

// virtual memory manager
struct vmman
{
    int (*init)(void);
    uintptr_t (*alloc)(size_t); // allocate an 'n'(kib), address is page aligned and size must be a multiple of 4Kib
    void (*free)(uintptr_t);    // deallocates a region of virtual memory
    size_t (*getfreesize)(void);      //returns available virtual memory space
    size_t (*getinuse)(void); // returns the size of used virtual memory space
};

extern struct vmman vmman;

int getpagesize(void);

#endif // VMM_H