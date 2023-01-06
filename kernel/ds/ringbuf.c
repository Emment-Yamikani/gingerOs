#include <ds/ringbuf.h>
#include <bits/errno.h>
#include <mm/kalloc.h>

int ringbuf_new(size_t size, ringbuf_t **rref)
{
    int err =0;
    char *buf = NULL;
    spinlock_t *lk = NULL;
    assert(rref, "no reference");

    if ((err = spinlock_init(NULL, "ringbuff", &lk)))
        return err;

    struct ringbuf *ring = kmalloc(sizeof(struct ringbuf));
    if (!ring) return -ENOMEM;

    if (!(buf = kmalloc(size)))
    {
        kfree(ring);
        spinlock_free(lk);
        return -ENOMEM;
    }

    *ring = (struct ringbuf)
    {
        .buf = buf,
        .size = size,
        .head = 0,
        .tail = 0,
        .lock = lk
    };

    *rref = ring;
    return 0;
}

void ringbuf_free(struct ringbuf *r)
{
    assert(r, "no ringbuff");
    spinlock_free(r->lock);
    kfree(r->buf);
    kfree(r);
}

int ringbuf_isempty(ringbuf_t *ring)
{
    ringbuf_assert(ring);
    ringbuf_assert_lock(ring);
    return ring->count == 0;
}

int ringbuf_isfull(ringbuf_t *ring)
{
    ringbuf_assert(ring);
    ringbuf_assert_lock(ring);
    return ring->count == ring->size;
}

size_t ringbuf_read(struct ringbuf *ring, size_t n, char *buf)
{
    size_t size = n;
    ringbuf_assert(ring);
    ringbuf_lock(ring);
    while (n)
    {
        if (ringbuf_isempty(ring))
            break;
        *buf++ = ring->buf[RINGBUF_INDEX(ring, ring->head++)];
        ring->count--;
        n--;
    }
    ringbuf_unlock(ring);
    return size - n;
}

size_t ringbuf_write(struct ringbuf *ring, size_t n, char *buf)
{
    size_t size = n;
    ringbuf_assert(ring);
    ringbuf_lock(ring);    
    while (n)
    {
        if (ringbuf_isfull(ring))
            break;

        ring->buf[RINGBUF_INDEX(ring, ring->tail++)] = *buf++;
        ring->count++;
        n--;
    }
    ringbuf_unlock(ring);
    return size - n;
}

size_t ringbuf_available(struct ringbuf *ring)
{
    ringbuf_assert(ring);
    ringbuf_lock(ring);    
    if (ring->tail >= ring->head)
    {
        ringbuf_unlock(ring);
        return ring->tail - ring->head;
    }
    size_t aval = ring->tail + ring->size - ring->head;
    ringbuf_unlock(ring);
    return aval;
}

void ringbuf_debug(ringbuf_t *ring)
{
    ringbuf_assert(ring);
    ringbuf_lock(ring);
    printk("size: %5d\nhead: %5d\ntail: %5d\ncount: %d\n",
        ring->size, RINGBUF_INDEX(ring, ring->head), RINGBUF_INDEX(ring, ring->tail), ring->count);
    ringbuf_unlock(ring);
}