#pragma once

#include <dev/dev.h>
#include <lib/stdint.h>
#include <lib/stddef.h>
#include <locks/cond.h>
#include <locks/mutex.h>

#define IOBUF_VALID 0x01    // I/O buffer contains valid data.
#define IOBUF_DIRTY 0x02    // I/O buffer needs synchronization.
#define IOBUF_BUSY  0x04    // I/O buffer is busy.

typedef struct iobuf
{
    int flags;        // I/O flags.
    long blkno;       // block Number.
    void *buf;        // Pointer to I/O buffer.
    int refcnt;       // Reference count.
    size_t blksz;     // Size of I/O buffer pointer to by 'iobuf'.
    struct devid *devid;// device associated with this buffer.
    mutex_t *lock;    // Lock.
} iobuf_t;

#define iobuf_assert(b) assert(b, "No I/O buffer")
#define iobuf_lock(b)  {iobuf_assert(b); mutex_lock(b->lock);}
#define iobuf_unlock(b)    {iobuf_assert(b); mutex_unlock(b->lock);}
#define iobuf_assert_locked(b)  {iobuf_assert(b); mutex_assert_lock(b->lock);}

#define __iobuf_setflag(b, f) (b->flags |= (f))
#define __iobuf_valid(b) (b->flags & IOBUF_VALID)
#define __iobuf_dirty(b) (b->flags & IOBUF_DIRTY)
#define __iobuf_busy(b)  (b->flags & IOBUF_BUSY)


int binit(void);
void bfree(void);
void bwrite(iobuf_t *buf);
void brelse(iobuf_t *buf);
iobuf_t *bget(struct devid *dd, long blkno, size_t size);
iobuf_t *bread(struct devid *devid, long blkno, size_t blksz);