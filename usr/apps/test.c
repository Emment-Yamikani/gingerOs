#include <ginger.h>

void *writer(void *);

int main(int argc, char *const argv[])
{
    tid_t tid = 0;

    thread_create(&tid, writer, NULL);
    sleep(2);
    unpark(tid);
    park();
    return 0;
}

void *writer(void *arg)
{
    printf("thread(%d), sleeping...\n", thread_self());
    park();
    printf("thread(%d), woken up\n", thread_self());
    exit(0);
    return NULL;
}