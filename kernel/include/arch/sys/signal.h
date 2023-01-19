#ifndef _ARCH_SIGNAL_H
#define _ARCH_SIGNAL_H 1

#include <sys/_signal.h>

int arch_handle_signal(int sig);
void arch_return_signal(void);

void handle_signals(void);

#endif //_ARCH_SIGNAL_H