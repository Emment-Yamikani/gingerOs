#pragma once

#include <lib/stdarg.h>
#include <lib/stddef.h>
//#include <locks/spinlock.h>

extern volatile int use_early_cons;

void init_printk(void);

int kvprintf(const char *__fmt__, va_list list);

int snprintf(char *s, size_t n, char *__fmt__, ...);
int vsnprintf(char *s, size_t n, char *__fmt__, va_list args);

int early_printf(const char *restrict __fmt__, ...);
int printk(const char *__fmt__, ...);
void panic(const char *__fmt__, ...);

#define KLOG_SILENT 0
#define KLOG_INIT   1
#define KLOG_OK     2
#define KLOG_FAIL   3
#define KLOG_WARN   4
#define KLOG_PANIC  5

int klog_init(int type, const char *restrict message);
int klog(int type, const char *restrict __fmt__, ...);