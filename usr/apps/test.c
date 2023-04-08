#include <ginger.h>

mutex_t *m = __mutex_init();

int main(int argc, char *const argv[])
{
    int fd = open("README.md", O_RDONLY);
    char buf[4096];
    read(fd, buf, sizeof buf);
    return 0;
}