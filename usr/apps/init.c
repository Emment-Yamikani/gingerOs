#include <ginger.h>

static int open_stdio(void)
{
    int fd = 0;

    if ((fd = open("/dev/kbd0", O_RDONLY)) < 0)
        return fd;
    if ((fd = open("/dev/console", O_WRONLY)) < 0)
    {
        close(0);
        return fd;
    }

    if ((fd = dup2(1, 2)) < 0)
    {
        close(0);
        close(1);
        return fd;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int err;
    pid_t pid = 0;
    int staloc = 0;

    char *argp[] = {"sh", NULL};

    if ((err = open_stdio()))
        return err;

    loop:
    switch (fork())
    {
    case -1:
        goto loop;
    case 0:
        setsid();
        exit(execv(*argp, argp));
        break;
    default:
        pid = wait(&staloc);
        goto loop;
    }    
    return 0;
}