#include <ginger.h>

void *writer(void *);

int main(int argc, char *const argv[])
{
    tid_t tid = 0;
    void *retval = NULL;
    char *_ptsname = NULL;
    int ptmx = open("/dev/ptmx", O_RDWR);
    grantpt(ptmx);
    unlockpt(ptmx);
    _ptsname = ptsname(ptmx);

    int pts = open(_ptsname, O_RDWR);
    
    printf("opened %s\n", _ptsname);
    return 0;
}
