#pragma once

// C('A') == Control-A
#define CTRL(x) (x - '@')

#define CHECK_FLG(flags, bit) (flags & (1 << bit))

/*Memory*/

//page size(4k)
#define PAGESZ  (0x1000)
//page size mask
#define PAGEMASK (PAGESZ - 1)

#define PGOFFSET(p) ((uintptr_t)(p) & PAGEMASK)
#define PGROUND(p) ((uintptr_t)(p) & ~PAGEMASK)
#define PGROUNDUP(sz) (((sz) + PAGEMASK) & ~(PAGEMASK))
#define GET_BOUNDARY_SIZE(p, s) (PGROUNDUP((p) + (s)) - PGROUND((p)))

//virtual memory base address
#define VMA_BASE  (0xC0000000)

#define VMA_HIGH(p) (((uintptr_t)(p)) + VMA_BASE)
#define VMA_LOW(p) (((uintptr_t)(p)) - VMA_BASE)

#define MMAP_DEVADDR (0xFE000000)
#define USER_STACK (0xC0000000)
#define KSTACKSIZE (0x8000)
#define USTACKSIZE (0x8000)
#define NTHREADS (0x2000)
#define USTACK_LIMIT (USER_STACK - (USTACKSIZE * NTHREADS))
#define TRAMPOLINE ((void *)0x8000)

#define _BS(b) ((1 << b))
#define _BITMASK(f, b) (f & (~b))
#define MIN(a, b) ((a < b) ? a : b)
#define SHL(a, b) (a << b)

#define foreach(elem, list) \
    for (typeof(*list) *tmp = list, elem = *tmp; elem; elem = *++tmp)

#define forlinked(elem, list, iter) \
    for (typeof(list) elem = list; elem; elem = iter)

#ifndef NELEM
#define NELEM(x) ((int)(sizeof(x) / sizeof(x[0])))
#endif // NELEM__ARRAY

#define LIME_DEBUG 0

/*Misc*/

#define __CAT(a, b) a##b

#define __fallthrough __attribute__((fallthrough))
#define __unused    __attribute((unused))
#define __packed    __attribute((packed))
#define __noreturn __attribute__((noreturn))
#define __align(n)     __attribute((align(n)))
#define __section(s) __attribute__((section(s)))

#define __cast_to(a) (a)
#define __cast_to_type(t) (typeof(t))
#define __cast_void (void *)

#define return_address(l) __builtin_return_address(l)

#define EARLY_INIT(name, i, f)                             \
    __section(".__einit") void *__CAT(__einit_, name) = i; \
    __section(".__efini") void *__CAT(__mfini_, name) = f;

static inline int __always() {return 1;}
static inline int __never() {return 0;}

void memory_usage(void);