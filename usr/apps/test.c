#include <ginger.h>

int v __align(0x1000);

int main(int argc, const char *argv[])
{
    int err = 0;
    void *addr = mmap(0, 0, 0, 0, 0, 0);
    printf("[test]: addr: %p\n", addr);

    err = munmap((void *)3, getpagesize());
    printf("err: %d\n", err);

    err = munmap(NULL, getpagesize());
    printf("err: %d\n", err);

    err = munmap((void *)&v, getpagesize());
    printf("err: %d, %p\n", err, &v);
    return 0;
}