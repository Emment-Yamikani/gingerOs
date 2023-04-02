#pragma once

#include <lib/types.h>

long sleep(long);

long wait_ms(long ms);

// sleep, waiting to be woken up by another thread.
int park(void);

// wake up a thread.
int unpark(tid_t tid);

void setpark(void);

int timed_wait(long ms);