#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/unistd.h>
#include <stdio.h>


int main(int argc, char *const argv[])
{
        int fd = open("wall.jpg", O_RDONLY);
	struct stat st;
	fstat(fd, &st);
	printf("size: %d\n", st.st_size);
	return 0;
}
