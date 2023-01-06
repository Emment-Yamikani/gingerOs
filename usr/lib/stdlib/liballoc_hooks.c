#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <locking/spinlock.h>
#include <unistd.h>


#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
# define MAP_ANONYMOUS MAP_ANON
#endif
#if !defined(MAP_FAILED)
# define MAP_FAILED ((char*)-1)
#endif

#ifndef MAP_NORESERVE
# ifdef MAP_AUTORESRV
#  define MAP_NORESERVE MAP_AUTORESRV
# else
#  define MAP_NORESERVE 0
# endif
#endif

static spinlock_t *spinlock = SPINLOCK_NEW("liballoc-spinlock");
static int page_size = -1;


int liballoc_lock()
{
	spin_lock(spinlock);
	return 0;
}

int liballoc_unlock()
{
	spin_unlock(spinlock);
	return 0;
}

void* liballoc_alloc( int pages )
{
	if ( page_size < 0 ) page_size = getpagesize();
	size_t size = pages * page_size;
	return (void *)sbrk(size);
}

//int dprintf(int, const char *, ...);

int liballoc_free( void* ptr, int pages )
{
	(void)ptr;
	(void)pages;
	dprintf(2, "warning: freeing memory\n");
	return 0;
}