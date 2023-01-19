#include <ginger.h>

void handler(int sig);

int main(int argc, char *argv[])
{
    int err = 0;
    int statloc = 0;
    void *ret = NULL;
    pid_t pid = 0, pgid = 0;
    int i = 0;

    signal(SIGINT, handler);
    kill(getpid(), SIGINT);
    thread_yield();
    exit(0);
    return 0;
}

void handler(int sig)
{
    printf("signal handler: %d\n", sig);
    for(;;);
}