#include <ginger.h>

int main(int argc, char *const argv[])
{
    int fd = open("/tmp/sync", O_RDWR);
    void *addr = mmap(NULL, 100, PROT_READ, MAP_PRIVATE, fd, 0);
    printf("%s: done executing: with: %s", __FILE__, addr);
    return 0;
}