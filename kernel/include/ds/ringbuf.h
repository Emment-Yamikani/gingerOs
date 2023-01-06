#pragma once 

#include <locks/spinlock.h>

#define RINGBUF_INDEX(ring, i) ((i) % ((ring)->size))

typedef struct ringbuf {
    char *buf;
    size_t size;
    size_t head;
    size_t tail;
    size_t count;
    spinlock_t *lock;
}ringbuf_t;

#define RINGBUF_NEW(nam, sz) (&(struct ringbuf){.buf = (char[sz]){0}, .size = sz, .head = 0, .tail = 0, .lock = SPINLOCK_NEW(nam)})

#define ringbuf_assert(r)   assert(r, "no ringbuf")
#define ringbuf_assert_lock(r)  spin_assert_lock(r->lock)
#define ringbuf_lock(r) spin_lock(r->lock)
#define ringbuf_unlock(r) spin_unlock(r->lock)

int ringbuf_isempty(ringbuf_t *);
int ringbuf_isfull(ringbuf_t *);
void ringbuf_debug(ringbuf_t *);
int ringbuf_new(size_t size, ringbuf_t **);
void ringbuf_free(struct ringbuf *r);
size_t ringbuf_read(struct ringbuf *ring, size_t n, char *buf);
size_t ringbuf_write(struct ringbuf *ring, size_t n, char *buf);
size_t ringbuf_available(struct ringbuf *ring);