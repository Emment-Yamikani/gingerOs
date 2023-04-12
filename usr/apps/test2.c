#include <ginger.h>

int main(int argc, char *const argv[])
{
    int fd = open("/she", O_RDONLY);
    void *addr = mmap(NULL, 100, PROT_READ, MAP_SHARED, fd, 0);

    printf("%s\n", addr);
    memcpy((addr + 35), "How is the weather\n", 20);
    return 0;
}