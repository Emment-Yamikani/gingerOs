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
pid_t getpgrp(void);
pid_t getpgid(pid_t pid);
pid_t setsid(void);
pid_t getsid(pid_t pid);
int setpgid(pid_t pid, pid_t pgid);
