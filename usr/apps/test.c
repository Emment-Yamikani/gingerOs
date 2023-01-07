#include <ginger.h>

int p[2] = {0};

void *thread1(void *arg);
void *thread2(void *arg);

tid_t th[2];

int main(int argc, char *argv[])
{
    void *ret = NULL;
    pipe(p);

    thread_create(&th[0], thread1, NULL);
    thread_create(&th[1], thread2, NULL);

    thread_join(th[0], &ret);
    thread_join(th[1], &ret);
    exit(0);
    return 0;
}

char *s = "Hello world\n";

void *thread1(void *arg)
{
    write(p[0], s, strlen(s));
    printf("TID: %d, done writing pipe\n", thread_self());
    close(p[0]);
    thread_exit(0);
    return NULL;
}

char buf[1024];

void *thread2(void *arg)
{
    read(p[1], buf, sizeof buf);
    printf("TID: %d, done reading pipe\n", thread_self());
    close(p[1]);
    thread_exit(0);
    return NULL;
}