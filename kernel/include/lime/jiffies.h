#ifndef LIME_JIFFIES_H

#define LIME_JIFFIES_H 1

#include <lib/stdint.h>
#include <lib/stddef.h>
#include <locks/spinlock.h>

typedef size_t jiffies_t;
typedef uint64_t jiffies64_t;

void jiffies_update(void);
jiffies_t jiffies_get(void);
jiffies64_t jiffies64_get(void);

#define time_after(unknown, known) ((long)(known) - (long)(unknown) < 0)
#define time_before(unknown, known) ((long)(unknown) - (long)(known) < 0)
#define time_after_eq(unknown, known) ((long)(unknown) - (long)(known) >= 0)
#define time_before_eq(unknown, known) ((long)(known) - (long)(unknown) >= 0)

#endif // LIME_JIFFIES_H