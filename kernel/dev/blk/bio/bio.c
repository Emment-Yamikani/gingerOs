#include <bits/errno.h>
#include <fs/fs.h>
#include <dev/bio.h>
#include <ds/queue.h>
#include <mm/kalloc.h>
#include <sys/thread.h>
#include <arch/i386/cpu.h>
#include <printk.h>

#define NBUF 1024
static iobuf_t *buffs = NULL;
static queue_t *iobufq = QUEUE_NEW("IO BUFFS");

void bfree(void)
{
    iobuf_t *b = NULL;

    queue_lock(iobufq);
    loop()
    {
        if ((b = dequeue(iobufq)))
        {
            if (b->lock)
                mutex_free(b->lock);
        }
        else
            break;
    }
    queue_unlock(iobufq);

    if (buffs)
        kfree(buffs);
}

int binit(void)
{
    int err = 0;
    int i_buf = 0;
    mutex_t *lock = NULL;

    klog(KLOG_INIT, "initializing I/O buffers, please wait...\n");
    return 0;

    if ((buffs = kcalloc(NBUF, sizeof *buffs)) == NULL)
        return -ENOMEM;

    loop()
    {
        if (i_buf >= NBUF)
            break;

        if ((err = mutex_init(NULL, &lock)))
        {
            bfree();
            return err;
        }

        buffs[i_buf].lock = lock;

        queue_lock(iobufq);
        if (enqueue(iobufq, &buffs[i_buf]) == NULL)
        {
            queue_unlock(iobufq);
            bfree();
            return -ENOMEM;
        }
        queue_unlock(iobufq);

        i_buf++;
    }

    klog(KLOG_OK, "I/O buffers ready.\n");
    return 0;
}

iobuf_t *bget(struct devid *devid, long blkno, size_t blksz)
{
    iobuf_t *b = NULL;
    if (current == NULL)
        return NULL; // No running thread CPU can bypass the buffer cache

    queue_lock(iobufq);
    forlinked(tmp, iobufq->head, tmp->next)
    {
        b = tmp->data;
        if (b->devid == devid && b->blkno == blkno)
        {
            if (b->blksz != blksz)
            {
                void *buf = NULL;
                if (b->buf == NULL)
                {
                    queue_unlock(iobufq);
                    return NULL;
                }

                if (b->blksz < blksz)
                {
                    if ((buf = krealloc(b->buf, blksz)) == NULL)
                    {
                        queue_unlock(iobufq);
                        return NULL;
                    }
                }
                b->buf = buf;
                b->blksz = blksz;
            }

            b->refcnt++;
            queue_unlock(iobufq);
            iobuf_lock(b);
            return b;
        }
    }

    forlinked(tmp, iobufq->tail, tmp->prev)
    {
        b = tmp->data;
        if (b->refcnt == 0)
        {
            if (b->blksz != blksz)
            {
                if (b->buf && (b->blksz < blksz))
                {
                    kfree(b->buf);
                    b->blksz = 0;
                    b->buf = NULL;
                }

                if ((b->buf == NULL))
                {
                    if ((b->buf = kcalloc(1, blksz)) == NULL)
                    {
                        queue_unlock(iobufq);
                        return NULL;
                    }
                    b->blksz = blksz;
                }
            }

            b->devid = devid;
            b->flags = 0;
            b->blkno = blkno;
            b->refcnt = 1;
            __iobuf_setflag(b, IOBUF_BUSY);
            queue_unlock(iobufq);
            iobuf_lock(b);
            return b;
        }
    }
    queue_unlock(iobufq);
    return NULL;
}

iobuf_t *bread(struct devid *devid, long blkno, size_t blksz)
{
    iobuf_t *b = NULL;
    dev_t *dev = NULL;

    if (devid == NULL)
        return NULL;

    printk("blkno: %d\n", blkno);

    if ((b = bget(devid, blkno, blksz)) == NULL)
        return NULL;

    if (!__iobuf_valid(b))
    {
        if ((dev = kdev_get(devid)) == NULL)
        {
            brelse(b);
            return NULL;
        }

        if (dev->devops.read == NULL)
        {
            brelse(b);
            return NULL;
        }
        
        printk("buff: %p: no: %d, sz: %d\n", b->buf, b->blkno, b->blksz);
        dev->devops.read(devid, blkno * b->blksz, b->buf, b->blksz);
        __iobuf_setflag(b, IOBUF_VALID);
    }

    return b;
}

void bwrite(iobuf_t *buf)
{
    dev_t *dev = NULL;
    iobuf_assert_locked(buf);

    if (!__iobuf_valid(buf))
        return;

    if ((dev = kdev_get(buf->devid)) == NULL)
    {
        brelse(buf);
        return;
    }

    if (dev->devops.write == NULL)
    {
        brelse(buf);
        return;
    }

    dev->devops.write(buf->devid, buf->blkno * buf->blksz, buf->buf, buf->blksz);
    __iobuf_setflag(buf, IOBUF_VALID);
}

void brelse(iobuf_t *buf)
{
    if (buf == NULL)
        return;
    
    iobuf_assert_locked(buf);

    iobuf_unlock(buf);

    queue_lock(iobufq);
    buf->refcnt--;

    if (buf->refcnt == 0)
    {
        if (buf->buf)
            kfree(buf->buf);
        buf->blkno = 0;
        buf->blksz = 0;
        buf->flags = 0;
        buf->buf = NULL;
        buf->devid = NULL;
        queue_remove(iobufq, buf);
        enqueue(iobufq, buf);
    }
    queue_unlock(iobufq);
}