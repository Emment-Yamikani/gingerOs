#pragma once

#include <lib/types.h>

uid_t getuid(void);
gid_t getgid(void);

int setuid(uid_t);
int setgid(gid_t);