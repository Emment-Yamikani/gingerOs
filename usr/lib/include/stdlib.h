#ifndef STDLIB_H
#define STDLIB_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    extern void free_tokens(char **ptr);
    extern char **canonicalize(const char *const path, int by);

    extern void *malloc(unsigned int);
    extern void *calloc(size_t, size_t);
    extern void *realloc(void *, unsigned int);
    extern void free(void *);
    extern void panic(const char *, ...);

#ifdef __cplusplus
}
#endif

#endif