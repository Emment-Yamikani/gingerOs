#pragma once

#include <lib/stdint.h>

typedef union page_flags
{
    struct
    {
        uint32_t exec : 1;     // executable?.
        uint32_t write : 1;    // writable?.
        uint32_t read : 1;     // readable?.
        uint32_t user : 1;     // user page.
        uint32_t valid : 1;    // page is valid in physical.
        uint32_t shared : 1;   // shared page.
        uint32_t dirty : 1;     // page is dirty
        uint32_t writeback : 1; // page needs writeback
        uint32_t can_swap : 1; // page swapping is allowed.
        uint32_t swapped : 1;  // page is swapped out.
        uint32_t mm_zone : 2;  // zone from which this page was allocated
    };
    uint32_t raw;
} page_flags_t;