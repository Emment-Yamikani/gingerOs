#include <ginger.h>

int main(int argc, const char *argv[])
{
    if (isatty(0) ==  1)
        printf("%s\n", ptsname(0));
    else
        panic("stdio is not a tty\n");
    return 0;
}