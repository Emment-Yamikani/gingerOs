#include <ginger.h>

int getstr(char *buf, int nbuf)
{
    memset(buf, 0, nbuf);
    gets(buf, nbuf);
    if (buf[0] == 0) // EOF
        return -1;
    return 0;
}

int parsestr(char *s)
{
    char buf[512];
    char *argv[] = {"sh", NULL};
    memset(buf, 0, sizeof buf);

    if (!strcmp(s, "guest"))
    {
        exit(0);
    }
    else if (!strcmp(s, "root"))
    {
        int ret =0, statloc =0;
        printf("password: ");
        while ((ret = getstr(buf, sizeof(buf))) >= 0)
        {
            char *pos = strchr(buf, '\n');
            if (pos)
                *pos = 0;
            if (!strncmp(buf, "<?$ygOsx554/.", strlen(buf)))
            {
                if (!fork())
                    execv(argv[0], argv);
                else{
                    wait(&statloc);
                    return statloc;
                }
            }
            else if (!strncmp(buf, "root", strlen(buf)))
            {
                if (!fork())
                    execv(argv[0], argv);
                else
                {
                    wait(&statloc);
                    return statloc;
                }
            }
            else
                printf("password incorrect\n");
            printf("password: ");
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    int ret;
    char buf[512];
    memset(buf, 0, sizeof buf);
    printf("login: ");
    while ((ret = getstr(buf, sizeof(buf))) >= 0)
    {
        char *pos = strchr(buf, '\n');
        if (pos)
            *pos = 0;
        parsestr(buf);
        printf("login: ");
    }
    exit(0);
    return 0;
}