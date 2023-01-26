#include <ginger.h>

int main(int argc, const char *argv[])
{
    char * s = NULL;
    if (isatty(0) ==  1)
        printf("%s\n", (s = ptsname(0)) ? s : "" );
    else
        panic("stdio is not a tty\n");
    return 0;
}