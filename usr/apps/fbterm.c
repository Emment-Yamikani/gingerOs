#include <ginger.h>

int pty = 0;
void *thrd_renderer(void *arg);

int main(int argc, char *const argv[], char *envp[])
{
    int err = 0;
    int pts = 0;
    tid_t tid = 0;
    pid_t shell = 0;
    close(0);
    close(1);
    close(2);
    open("/dev/kbd0", O_RDONLY);
    dup2(open("/dev/console", O_WRONLY), 2);
    pty = open("/dev/ptmx", O_RDWR);
    grantpt(pty);
    unlockpt(pty);
    dup2(pty, 2);

    thread_create(&tid, thrd_renderer, NULL);

    if ((shell = fork()) < 0)
        panic("failed to start shell");
    else if (shell == 0)
    {
    again:
        if ((shell = fork()) == 0)
        {
            setsid();
            close(0);
            close(1);
            close(2);
            pts = open(ptsname(pty), O_RDWR);
            dup2(pts, 1);
            dup2(pts, 2);
            char *argp[] = {"shell", NULL};
            panic("execve(%s): failed, err: %d\n", *argp, execv(*argp, argp));
        }
        else {
            wait(NULL); goto again;
        }
    }
    

    for (;;)
    {
        char buf[1];
        ssize_t len = read(0, buf, sizeof buf);
        write(pty, buf, len);
    }
}


void *thrd_renderer(void *arg)
{
    char buf[1];
    for (;;)
    {
        ssize_t len = read(pty, buf, sizeof buf);
        write(1, buf, len);
    }
}