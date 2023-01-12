#pragma once

#include <lib/types.h>
#include <lib/stddef.h>

pid_t getpid(void);

void exit(int);
pid_t fork(void);
pid_t wait(int *);
int execv(char *, char *[]);
int execve(char *, char *[], char *[]);
extern int brk(void *addr);
extern void *sbrk(ptrdiff_t nbytes);


pid_t getppid(void);

int setpgrp(void);

