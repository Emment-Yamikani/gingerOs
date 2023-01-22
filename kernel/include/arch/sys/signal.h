#ifndef _ARCH_SIGNAL_H
#define _ARCH_SIGNAL_H 1

#include <sys/_signal.h>


int arch_handle_signal(int sig, trapframe_t *);
void arch_return_signal(trapframe_t *);

#endif //_ARCH_SIGNAL_H