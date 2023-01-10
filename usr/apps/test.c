#include <ginger.h>

int p[2] = {0};

void *thread1(void *arg);
void *thread2(void *arg);

tid_t th[2];

int main(int argc, char *argv[])
{
    int statloc =0;
    pid_t pid = 0;
    void *ret = NULL;

    pipe(p);
    printf("pipe[%d:%d]\n", p[0], p[1]);

    if (thread_create(&th[0], thread1, NULL) < 0)
        panic("failed to create thread\n");

    if (thread_create(&th[1], thread2, NULL) < 0)
        panic("failed to create thread\n");

    thread_join(th[0], &ret);
    thread_join(th[1], &ret);


    exit(0);
    return 0;
}

char *s = "Hello world";

void *thread1(void *arg)
{
    write(p[1], s, strlen(s));
    close(p[1]);
    thread_exit(0);
    return NULL;
}

char buf[1024];

void *thread2(void *arg)
{
    char buf[1024];
    read(p[0], buf, sizeof buf);
    close(p[0]);
    printf("read: %s\n", buf);
    thread_exit(0);
    return NULL;
}