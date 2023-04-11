#include <ginger.h>

mutex_t *m = __mutex_init();

    char buf [1024] = {0};
int main(int argc, char *const argv[])
{
    int fd = open("/she", O_RDONLY);
    memset(buf, 0, sizeof buf);
    read(fd, buf, sizeof buf);
    printf("%s\n", buf);
    return 0;
}