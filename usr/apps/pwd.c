#include <ginger.h>

int main(int argc, char const *argv[])
{
    char buf[1024];
    memset(buf, 0, sizeof buf);
    if (!getcwd(buf, sizeof buf))
        panic("couldn't get cwd, might be too long\n");
    printf("%s\n", buf);
    exit(0);
    return 0;
}
