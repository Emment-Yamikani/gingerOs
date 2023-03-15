#pragma once

#include <lib/stdint.h>
#include <lib/stddef.h>

typedef struct page_shadow
{
    uintptr_t virt; // virtual page
    uintptr_t phys; // physical page frame
    union {
        struct {
            uint8_t p:1; // present?
            uint8_t w:1; // writable?
            uint8_t u:1; // user?
            uint8_t d:1; // dirty
        };
        uint8_t raw;
    }flags;
} page_shadow_t;