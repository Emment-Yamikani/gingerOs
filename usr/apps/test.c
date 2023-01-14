#include <ginger.h>

int main(int argc, char *argv[])
{
    int statloc =0;
    pid_t pid = 0;
    void *ret = NULL;
    
    if ((pid = fork()))
    {
    }
    else
    {
        sleep(2);
    }
    exit(0);
    return 0;
}