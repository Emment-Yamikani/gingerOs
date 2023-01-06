#pragma once

#include <printk.h>

#define assert(condition, msg) do if (!(condition)) panic("%s:%d: in function %s() \e[0;4m%s, \e[0;012mreturn [\e[0;015m%p\e[0m]\e[0m\n", __FILE__, __LINE__, __func__, msg, return_address(0)); while (0)