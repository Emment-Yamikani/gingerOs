#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

static int page_size = -1;


int liballoc_lock()
{
	// implement mutex
	return 0;
}

int liballoc_unlock()
{
	// implement mutex
	return 0;
}

void* liballoc_alloc( int pages )
{
	if ( page_size < 0 ) page_size = getpagesize();
	unsigned int size = pages * page_size;
	
	char *p2 = (char*)sbrk(size);

	if ( !p2 ) return NULL;

	return p2;
}

int liballoc_free( void* ptr, int pages )
{
	return 0; //munmap( ptr, pages * page_size );
}



