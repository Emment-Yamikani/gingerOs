#include <stddef.h>
#include <unistd.h>
#include <stdio.h>


typedef union header {
    struct{
        union header * ptr;
        unsigned size;
    }s;
    long _;
}Header;

static Header base;
static Header *freep = (Header*)NULL;

static Header *morecore(size_t);
void * malloc(size_t nbytes)
{
    dprintf(0, "malloc()x\n");
    Header *p, *prevp;
    size_t nunits;
    nunits = (nbytes + sizeof(Header)-1) / sizeof(Header) + 1;
    dprintf(0, "malloc(%d)\n", nbytes);

    if((prevp = freep) == NULL)
    {
        base.s.ptr = freep = prevp = &base;
        base.s.size = 0;
    }

    dprintf(0, "malloc(%d)\n", nbytes);
    for(p = prevp->s.ptr; ; prevp = p, p = p->s.ptr){
        dprintf(0, "malloc()\n");
        if(p->s.size >= nunits){
            if (p->s.size == nunits)
                prevp->s.ptr = p->s.ptr;
            else{
                p->s.size -= nunits;
                p += p->s.size;
                p->s.size = nunits;
            }
            freep = prevp;
            return (void *)( p +1);
        }
        if (p == freep)
            if ((p = morecore(nunits)) == NULL)
                return NULL;
    }
}

#define NALLOC 1024
void free(void *ap);
static Header * morecore(size_t nu)
{
    char *cp;
    Header *up;
    if (nu < NALLOC)
        nu = NALLOC;
    
    dprintf(0, "morecore\n");
    cp = (char *)sbrk(nu * sizeof(Header));
    
    if (cp == (char *)-1)
        return NULL;
    up = (Header *)cp;
    up->s.size = nu;
    free((void *)(up + 1));
    return freep;
}

void *calloc(size_t nelem, size_t szelem)
{
    return malloc(nelem * szelem);
}

void *relloc(void * pre, unsigned int newsize)
{
    void *buf = malloc(newsize);
    if (!buf)
        return NULL;
    free(pre);
    return buf;
}

void free(void *ap)
{
    Header *bp, *p;
    bp = (Header *)ap -1;
    for(p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
        if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))
            break;
    if (bp + bp->s.size == p->s.ptr){
        bp->s.size += p->s.ptr->s.size;
        bp->s.ptr = p->s.ptr->s.ptr;
    }
    else
        bp->s.ptr = p->s.ptr;
    if( p + p->s.size == bp){
        p->s.size += bp->s.size;
        p->s.ptr = bp->s.ptr;
    }
    else
        p->s.ptr = bp;
    freep = p;
}