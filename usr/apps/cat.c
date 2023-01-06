#include <ginger.h>

static void usage(char *name)
{
    printf("Usage: %s [-u] [file...]\n\
            Concatenate and print files\n");
}

char buf[1024];

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        int fd = 0; /* Use stdin by default */

        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
                case 'u':
                    /* ignore */
                    goto next;
                case '\0':
                    break;
                default:
                    usage(argv[0]);
                    return -1;
            }
        } else {
            fd = open(argv[i], O_RDONLY);

            if (fd < 0) {
                dprintf(2, "Error opening file %s: ", argv[i]);
            }
        }


        int r;
        while ((r = read(fd, buf, 1024)) > 0) {
            write(1, buf, r);
        }

        close(fd);
next:;
    }
    exit(0);
    return 0;
}
