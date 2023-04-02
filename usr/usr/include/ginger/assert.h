#pragma once

#include <stdio.h>
#include <stdlib.h>

#define assert(condition, msg) do if (!(condition)) panic("%s:%d: in function %s() %s, return [%p]\n", __FILE__, __LINE__, __func__, msg, __builtin_return_address(0)); while (0)