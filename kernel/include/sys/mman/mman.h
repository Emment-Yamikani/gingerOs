#pragma once

#include <lib/types.h>
#include <lib/stdint.h>
#include <lib/stddef.h>

#define MAP_FAILED    ((void *) -1)   // mapping error occured

void *mmap(void *addr, size_t length, int prot, int flags,
           int fd, off_t offset);

int munmap(void *addr, size_t length);
int mprotect(void *addr, size_t len, int prot);