#ifndef __STRING__
#define __STRING__

int strlen(const char *s);
char *strdup(const char *s);
void strcat(char *s, char *t);
int strcmp(char *s1, char *s2);
void strcpy(char *__dst, char *__src);
char *strncpy(char *s, const char *t, int n);
char *safestrcpy(char *s, const char *t, int n);
int strncmp(const char *p, const char *q, unsigned int n);


void *memset(void *dst, int c, unsigned int n);
void *memcpyd(void *restrict dstptr, const void *restrict srcptr, unsigned int size);
void *memsetw(void *dst, int c, unsigned int n);
void *memsetd(void *dst, int c, unsigned int n);
void *memmove(void *dst, const void *src, unsigned int n);
void *memmoved(void *dst, const void *src, unsigned int n);
void *memcpy(void *restrict dstptr, const void *restrict srcptr, unsigned int size);

char **tokenize(const char *s, char c);
char **canonicalize_path(const char *const path);
void tokens_free(char **ptr);

#endif