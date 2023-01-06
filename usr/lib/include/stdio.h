#ifndef _STDIO_H
#define _STDIO_H 1

#include <sys/cdefs.h>
#include <stdarg.h>

//End of file
#define EOF (-1)

#include <snprintf.h>

#ifdef __cplusplus
extern "C" {
#endif

//printf(char*, ...)
extern int printf(const char* __restrict, ...);
//print single char
extern int putchar(int);
//print a string of text
extern int puts(const char*);

extern int vdprintf(int fd, const char *format, va_list list);
extern int vprintf(const char *format, va_list list);
extern int dprintf(int fd, char *fmt, ...);
extern int printf(const char *restrict fmt, ...);

extern char *gets(char *, int max);
extern int getchar(void);

int printk(const char *restrict fmt, ...);

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#ifdef __cplusplus
}
#endif

#endif
