#include <lib/stdint.h>

void *
memset(void *dst, int c, uint32_t n)
{
    char *d;

    d = (char *)dst;
    while (n-- > 0)
        *d++ = c;

    return dst;
}

int 
memcmp(const void *v1, const void *v2, uint32_t n)
{
    const uint8_t *s1, *s2;

    s1 = v1;
    s2 = v2;
    while (n-- > 0)
    {
        if (*s1 != *s2)
            return *s1 - *s2;
        s1++, s2++;
    }

    return 0;
}

void *
memcpy(void *restrict dstptr, const void *restrict srcptr, unsigned int size)
{
    unsigned char *dst = (unsigned char *)dstptr;
    const unsigned char *src = (const unsigned char *)srcptr;
    unsigned int i;
    for (i = 0; i < size; i++)
        dst[i] = src[i];
    return (void *)&dst[i];
}

void *
memcpyd(void *restrict dstptr, const void *restrict srcptr, unsigned int size)
{
    unsigned int *dst = (unsigned int *)dstptr;
    const unsigned int *src = (const unsigned int *)srcptr;
    unsigned int i;
    for (i = 0; i < size; i++)
        dst[i] = src[i];
    return (void *)&dst[i];
}

void *
memmove(void *dst, const void *src, uint32_t n)
{
    const char *s;
    char *d;

    s = src;
    d = dst;
    if (s < d && s + n > d)
    {
        s += n;
        d += n;
        while (n-- > 0)
            *--d = *--s;
    }
    else
        while (n-- > 0)
            *d++ = *s++;

    return dst;
}

void *
memmoved(void *dst, const void *src, uint32_t n)
{
    const int *s;
    int *d;

    s = src;
    d = dst;
    if (s < d && s + n > d)
    {
        s += n;
        d += n;
        while (n-- > 0)
            *--d = *--s;
    }
    else
        while (n-- > 0)
            *d++ = *s++;

    return dst;
}

void *
memsetw(void *dst, int c, unsigned int n)
{
    unsigned short *d;

    d = (unsigned short *)dst;
    while (n-- > 0)
        *d++ = c;

    return dst;
}

void *
memsetd(void *dst, int c, unsigned int n)
{
    unsigned int *d;

    d = (unsigned int *)dst;
    while (n-- > 0)
        *d++ = c;

    return dst;
}

int strlen(const char *s)
{
    int n;

    for (n = 0; s[n]; n++)
        ;
    return n;
}

void
strcat(char *s, char *t)
{
    int i = 0, j = 0;
    while (s[i])
        i++;
    while ((s[i++] = t[j++]))
        ;
}

void
strcpy(char *__dst, char *__src)
{
    if (__src && __dst)
        while ((*__dst++ = *__src++));
}

//#include <printk.h>

int strcmp(const char *p, const char *q)
{
    while (*p && *p == *q)
        p++, q++;
    return (uint8_t)*p - (uint8_t)*q;
}

int
strncmp(const char *p, const char *q, uint32_t n)
{
    while (n > 0 && *p && *p == *q)
        n--, p++, q++;
    if (n == 0)
        return 0;
    return (uint8_t)*p - (uint8_t)*q;
}

char *
strncpy(char *s, const char *t, int n)
{
    char *os;

    os = s;
    while (n-- > 0 && (*s++ = *t++) != 0)
        ;
    while (n-- > 0)
        *s++ = 0;
    return os;
}

// Like strncpy but guaranteed to NUL-terminate.
char *
safestrcpy(char *s, const char *t, int n)
{
    char *os;

    os = s;
    if (n <= 0)
        return os;
    while (--n > 0 && (*s++ = *t++) != 0)
        ;
    *s = 0;
    return os;
}

int compare_strings(const char *s0, const char *s1)
{
    if (strlen(s0) != strlen(s1))
        return -1;
    return strcmp((char *)s0, (char *)s1);
}

#include <mm/kalloc.h>
#include <sys/system.h>


#ifdef KALLOC_H
char *
strdup(const char *s)
{
    int len = strlen((const char *)s) + 1;
    char *str = (char *)kmalloc(len);
    if (!str)
        return (char *)0;
    strcpy(str, (char *)s);
    str[len -1] = 0;
    return str;
}

static char *
grabtok(char *s, int *rlen, int c)
{
    int len = 0;
    char *tok = NULL, *buf = NULL;
    if (!s)
        return NULL;
    buf = s;
    while (*s && (*s++ != c))
        len++;
    s = buf;
    tok = kmalloc(len + 1);
    memset(tok, 0, len + 1);
    safestrcpy(tok, s, len + 1);
    if (rlen)
        *rlen = len;
    return tok;
}

char **
tokenize(const char *s, int c)
{
    int i = 0;
    int len = 0;
    char *buf = NULL;
    char **tokens = NULL;
    char *tmp = NULL, *tmp2 = NULL;

    if (!s || !c)
        return NULL;
    len = strlen(s);
    buf = kmalloc(len + 1);
    memset(buf, 0, len + 1);
    safestrcpy(buf, s, len + 1);
    tmp = buf;
    tmp2 = &buf[strlen(buf) - 1];
    while (*tmp2 && (*tmp2 == c))
        *tmp2-- = '\0';
    for (int len = 0; *buf; ++i)
    {
        tokens = krealloc(tokens, (sizeof(char *) * (i + 1)));
        while (*buf && (*buf == c))
            buf++;
        char *tok = grabtok(buf, &len, c);
        if (!tok)
            break;
        tokens[i] = tok;
        buf += len;
    }
    tokens = krealloc(tokens, (sizeof(char *) * (i + 1)));
    tokens[i] = NULL;
    kfree(tmp);
    return tokens;
}

void tokens_free(char **tokens)
{
    if (!tokens)
        return;
    foreach (token, tokens)
        kfree(token);
    kfree(tokens);
    return;
}

/*
char **
tokenize(const char *s, char c)
{
    if (!s || !*s)
        return NULL;

    while (*s == c)
        ++s;

    char *tokens = strdup(s);
    int len = strlen((char *)s);

    if (!len)
    {
        char **ret = kmalloc(sizeof(char *));
        *ret = NULL;
        return ret;
    }

    int i, count = 0;
    for (i = 0; i < len; ++i)
    {
        if (tokens[i] == c)
        {
            tokens[i] = 0;
            ++count;
        }
    }

    if (s[len - 1] != c)
        ++count;

    char **ret = kmalloc(sizeof(char *) * (count + 1));

    int j = 0;
    ret[j++] = tokens;

    for (i = 0; i < strlen((char *)s) - 1; ++i)
        if (tokens[i] == 0)
            ret[j++] = &tokens[i + 1];

    ret[j] = NULL;

    return ret;
}

void free_tokens(char **ptr)
{
    if (!ptr)
        return;
    if (*ptr)
        kfree(*ptr);
    kfree(ptr);
}*/

char **canonicalize_path(const char *path)
{
    /* Tokenize slash seperated words in path into tokens */
    char **tokens = tokenize(path, '/');
    return tokens;
}

char *combine_strings(const char *s0, const char *s1)
{
    if (!s0 || !s1)
        return NULL;

    size_t string_len = strlen(s0) + strlen(s1) + 1;
    char *string = (typeof(string))kmalloc(string_len);

    if (!string)
        return NULL;

    memset(string, 0, string_len);
    strcat(string, (char *)s0);
    strcat(string, (char *)s1);
    return string;
}

#include <lib/ctype.h>

char *strcat_num(char *str, int num, uint8_t base)
{
    char ii[11] = {0};
    itostr(ii, num, base);
    return combine_strings(str, ii);
}

void ginger_memclear(void *ptr, int len)
{
    memset(ptr, 0, len);
}

#endif //KMALLOC