#include <ginger.h>

void print_help(void);
void print_missing_op(void);
void print_invalid(const char *s);

int main(int argc, char *const argv[])
{
    long sec = 0;
    char *num = NULL, *unit = NULL;

    if (argc <= 0)
        print_missing_op();

    for (int i = 1, j = 0; i < argc; ++i)
    {
        if (strlen(argv[i]) == 0)
            continue;

        if (isdigit(*argv[i]))
            num = argv[i];
        else
            print_invalid(argv[i]);

        for (j = 0; argv[i][j]; ++j)
        {
            if (!isdigit(argv[i][j]))
            {
                unit = &argv[i][j];
                break;
            }
        }

        if (unit)
        {
            if (strlen(unit) > 1)
                print_invalid(argv[i]);
            int p = j - 1;
            int k = 0;
            long pos = 0;
            long val = 0;

            if ((*unit != 's') && (*unit != 'm') && (*unit != 'h') && (*unit != 'd'))
                print_invalid(argv[i]);

            while (j--)
            {
                pos = 1;
                for (k = 0; k < p - j; ++k)
                    pos *= 10;
                val += ((argv[i][j] - 0x30) * pos);
            }

            switch (*unit)
            {
            case 's':
                sec += val;
                break;
            case 'm':
                sec += val * 60;
                break;
            case 'h':
                sec += val * 60 * 60;
                break;
            case 'd':
                sec += val * 24 * 60 * 60;
                break;
            }
        }
        else
        {
            int p = j - 1;
            int k = 0;
            long pos = 0;
            long val = 0;

            while (j--)
            {
                pos = 1;
                for (k = 0; k < p - j; ++k)
                    pos *= 10;
                sec += ((argv[i][j] - 0x30) * pos);
            }
        }
    }

    sleep(sec);
    exit(0);
    return 0;
}

void print_invalid(const char *s)
{
    dprintf(2, "sleep: invalid time interval '%s'.\n\n", s);
    print_help();
    exit(1);
}

void print_help(void)
{
    dprintf(2, "Try 'sleep --help' for more information.\n");
}

void print_missing_op(void)
{
    dprintf(2, "sleep: missing operand(s).\n\n");
    print_help();
    exit(2);
}