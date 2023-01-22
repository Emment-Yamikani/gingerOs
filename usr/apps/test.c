#include <ginger.h>

void handler(int sig);

void *t1(void *arg)
{
    printf("%s:%d: TID: %d running\n", __FILE__, __LINE__, thread_self());
    while (1);
}

void *t2(void *arg)
{
    printf("%s:%d: TID: %d running\n", __FILE__, __LINE__, thread_self());
    while (1)
        ;
}

int main(int argc, char *argv[])
{
    int err = 0;
    int statloc = 0;
    void *ret = NULL;
    pid_t pid = 0, pgid = 0;
    int i = 0;

    thread_create(&pid, t1, NULL);

    signal(SIGINT, handler);
    //kill(getpid(), SIGINT);

    pid = fork();

    if (pid == 0)
    {
        signal(SIGINT, handler);
        thread_create(&pid, t2, NULL);
        kill(getppid(), SIGINT);
        for(;;);
    }


    sleep(4);
    kill(pid, SIGINT);

    err = kill(pid, 0);


    printf("PID: %d \"%s\" send signal to PID: %d\n", getpid(), err ? "can't" : "can", pid);
    kill(pid, SIGINT);
    exit(0);
    return 0;
}

void handler(int sig)
{
    signal(SIGINT, handler);
    printf("TID: %d, signal handler: %d, %p\n", thread_self(), sig, __builtin_return_address(0));
}