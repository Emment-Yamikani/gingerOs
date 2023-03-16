#include <ginger.h>

int main (int argc, char *const argv[]) {
    int err = mprotect(mmap(NULL, 4096, PROT_READ,
        MAP_PRIVATE | MAP_ANON, -1, 0), 4096, PROT_READ | PROT_WRITE);
    
    printf("err: %d\n", err);
    return 0;
}