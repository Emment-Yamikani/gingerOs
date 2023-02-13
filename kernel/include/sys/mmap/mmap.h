#pragma once

#include <lib/types.h>
#include <lib/stdint.h>
#include <lib/stddef.h>


void *mmap(void *addr, size_t length, int prot, int flags,
           int fd, off_t offset);

int munmap(void *addr, size_t length);
