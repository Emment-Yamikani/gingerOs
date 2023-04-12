#include <ginger.h>

mutex_t *m = __mutex_init();

    char buf [1024] = {0};
int main(int argc, char *const argv[])
{
    int fd = open("/she", O_RDONLY);
    void *addr = mmap(NULL, 100, PROT_READ, MAP_SHARED, fd, 0);

    if (!fork())
        exit(execv("/test2", NULL));

    sleep(5);
    printf("data: %s\n", addr);
    return 0;
}