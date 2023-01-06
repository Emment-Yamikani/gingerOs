#ifndef _STRING_H
#define _STRING_H 1

#include "sys/cdefs.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    extern long atol(const char * s);
    extern int atoi(const char * s);

    extern void *memcpy(void *restrict dstptr, const void *restrict srcptr, size_t size);
    extern void *memset(void *bufptr, int value, size_t size);
    extern int memcmp(const void * vl, const void * vr, size_t n);
    extern void * memchr(const void * src, int c, size_t n);
    extern void * memrchr(const void * m, int c, size_t n);
    extern void *memmove(void *dest, const void *src, size_t n);

    extern size_t lfind(const char * str, const char accept);
    extern size_t rfind(const char * str, const char accept);

    extern int strncmp(const char *s1, const char *s2, size_t n);
    extern char * stpcpy(char * restrict d, const char * restrict s);
    extern char *strncpy(char *dest, const char *src, size_t n);
    extern size_t strxfrm(char *dest, const char *src, size_t n);
    extern char * strchr(const char * s, int c);
    extern int strcmp(const char * l, const char * r);
    extern char * strcpy(char * restrict dest, const char * restrict src);
    extern char * strdup(const char * s);
    extern size_t strlen(const char * s);
    extern char * strtok_r(char * str, const char * delim, char ** saveptr);
    extern size_t strspn(const char * s, const char * c);
    extern char * strchrnul(const char * s, int c);
    extern char * strrchr(const char * s, int c);
    extern size_t strcspn(const char * s, const char * c);
    extern char * strpbrk(const char * s, const char * b);
    extern char *strstr(const char * h, const char * n);
    extern char * strtok(char * str, const char * delim);
    extern char * strcat(char *dest, const char *src);
    extern char * strncat(char *dest, const char *src, size_t n);

#ifdef __cplusplus
}
#endif

#endif
