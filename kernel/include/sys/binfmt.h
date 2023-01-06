#pragma once

#include <lib/stdint.h>
#include <sys/proc.h>

int binfmt_load(const char *, proc_t *, proc_t **);