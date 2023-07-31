#include <ginger.h>

mutex_t *m = __mutex_init();

char buf [1024] = {0};
int main(int argc, char *const argv[])
{
    int fd = open("/tmp/sync", O_RDWR | O_CREAT, 0777);
    void *addr = mmap(NULL, 100, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    memcpy(addr, "Foolish Code\n", 14);
    if (!fork()) exit(execv("test2", NULL));
    return 0;
}