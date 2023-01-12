#include <ginger.h>

int main(int argc, char *argv[])
{
    int statloc =0;
    pid_t pid = 0;
    void *ret = NULL;

    if (setpgrp() < 0)
        panic("error setting pgrp\n");

    exit(0);
    return 0;
}