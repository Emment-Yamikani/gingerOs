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
    int err = 0;
    tid_t tid = 0;
    pid_t pid = 0;
    int staloc = 0;

    char *argp[] = {"sh", NULL};

    if ((err = open_stdio()))
        return err;


    loop:
    if ((pid = fork()) < 0)
        panic("failed to fork a child\n");
    else
    if (pid > 0)
    {
        if (fork() == 0)
        {
            char *fb[] = {"/fbdev", "art.jpg", NULL};
            execv(fb[0], fb);
            panic("failed to set wallpaper\n");
        }
        haschild:
        pid = wait(&staloc);
        if (pid < 0)
            goto loop;
        goto haschild;
    }
    else{
        setsid();
        execv(argp[0], argp);
        panic("execv returned\n");
    }

    exit(0);
}