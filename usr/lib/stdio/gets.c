#include <stdio.h>
#include <unistd.h>

char *gets(char *buf, int max)
{
    int i, cc;
    char c;

    for (i = 0; i + 1 < max;)
    {
        cc = read(STDIN_FILENO, &c, 1);
        if (cc < 1)
            break;
        buf[i++] = c;
        if (c == '\n' || c == '\r')
            break;
    }
    buf[i] = '\0';
    return buf;
}

int getchar(void)
{
    int c = 0;
    read(STDIN_FILENO, &c, 1);
    return c;
}