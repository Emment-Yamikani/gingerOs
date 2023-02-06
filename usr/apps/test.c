#include <ginger.h>

int pty = 0;
int ptmx = 0;
int prog_exit = 0;

void handler(int sig);
void *keyboard_thread(void *);
void *screen_thread(void *);

int main(int argc, char *argv[])
{
    int i = 0;
    int err = 0;
    int statloc = 0;
    void *ret = NULL;
    char *pts = NULL;
    tid_t tid[10] = {0};
    pid_t pid = 0, pgid = 0;

    ptmx = open("/dev/ptmx", O_RDWR);

    if (ptmx < 0) panic("failed to open ptmx\n");

    pts = ptsname(ptmx);

    if ((err = grantpt(ptmx)))
        panic("grantpt(): failed with error: %d\n", err);

    if ((err = unlockpt(ptmx)))
        panic("unlockpt(): failed with error: %d\n", err);


    if ((pid = fork()) == 0)
    {
        if ((pty = open(pts, O_RDWR)) < 0)
            panic("open(): failed with error: %d\n", pty);

        dup2(pty, 0);
        dup2(pty, 1);
        dup2(pty, 2);
        close(pty);
        close(ptmx);
        
        char *argp[] = {"/mnt/ramfs/fbdev", "gsd.jpg", NULL};
        execv(argp[0], argp);

        kill(getppid(), SIGINT);
        panic("failed to exec fbdev");
    }
    
    if ((err = thread_create(&tid[1], screen_thread, NULL)))
        panic("thread_create(): failed with error: %d\n", err);

    if ((err = thread_create(&tid[0], keyboard_thread, NULL)))
        panic("thread_create(): failed with error: %d\n", err);

    wait(&statloc);
    prog_exit = 1;
    
    exit(statloc);
    return 0;
}

void *keyboard_thread(void *arg)
{
    (void)arg;
    char buf[1];

    while (1)
    {
        if (prog_exit)
            thread_exit((void *)prog_exit);
        memset(buf, 0, sizeof buf);
        read(0, buf, sizeof buf);
        if (prog_exit)
            thread_exit((void *)prog_exit);
        write(ptmx, buf, sizeof buf);
    }
}

void *screen_thread(void *arg)
{
    (void)arg;
    char buf[1];
    while (1)
    {
        if (prog_exit)
            thread_exit((void *)prog_exit);
        memset(buf, 0, sizeof buf);
        read(ptmx, buf, sizeof buf);
        if (prog_exit)
            thread_exit((void *)prog_exit);
        write(1, buf, sizeof buf);
    }
}

void handler(int sig)
{
}