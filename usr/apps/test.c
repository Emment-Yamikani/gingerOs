#include <ginger.h>

int main(int argc, char *argv[])
{
    int statloc =0;
    pid_t pid = 0;
    void *ret = NULL;
    
    if ((pid = fork()))
    {
        printf("parent\n");
    }
    else
    {
        printf("child\n");
        sleep(2);
    }
    exit(0);
    return 0;
}