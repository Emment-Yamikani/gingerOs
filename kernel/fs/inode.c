#include <bits/errno.h>
#include <fs/fs.h>
#include <lib/string.h>
#include <lime/string.h>
#include <mm/kalloc.h>
#include <printk.h>

static iops_t iops;

int ialloc(inode_t **ref)
{
    int err =0;
    inode_t *ip = NULL;
    spinlock_t *lock = NULL;
    cond_t *reader_wait = NULL, *writer_wait = NULL;

    if (!ref)
        return -EINVAL;
    
    if (!(ip = __cast_to_type(ip)kmalloc(sizeof *ip)))
    {
        err = -ENOMEM;
        goto error;
    }

    if ((err = spinlock_init(NULL, "inode", &lock)))
        goto error;

    if ((err = cond_init(NULL, "inode-reader", &reader_wait)))
        goto error;
    if ((err = cond_init(NULL, "inode-writer", &writer_wait)))
        goto error;
    
    memset(ip, 0, sizeof *ip);

    ip->i_lock = lock;
    ip->i_rwait = reader_wait;
    ip->i_rwait = writer_wait;
    ip->iops = iops;

    *ref = ip;
    return 0;
error:
    if (ip)
        kfree(ip);
    if (lock)
        spinlock_free(lock);
    if (reader_wait)
        cond_free(reader_wait);
    if (writer_wait)
        cond_free(writer_wait);
    printk("ialloc(): called @ 0x%p, error=%d\n", return_address(0), err);
    return err;
}

void ilock(inode_t *ip)
{
    if (!ip)
        panic("ilock(), called @ 0x%p, no inode to lock\n", return_address(0));
    spin_lock(ip->i_lock);
}

void iunlock(inode_t *ip)
{
    if (!ip)
        panic("iunlock(), called @ 0x%p, no inode to unlock\n", return_address(0));
    spin_unlock(ip->i_lock);
}

int iincrement(inode_t *inode)
{
    if (!inode)
        return -EINVAL;
    ilock(inode);
    inode->i_refs++;
    iunlock(inode);
    return 0;
}

int irelease(inode_t *ip)
{
    int err = 0;
    if (!ip)
    {
        err = -EINVAL;
        goto error;
    }
    
    ilock(ip);
    if (ip->i_refs > 0)
    {
        err = -EBUSY;
        goto error;
    }
    iunlock(ip);

    spinlock_free(ip->i_lock);
    cond_free(ip->i_rwait);
    cond_free(ip->i_wwait);
    kfree(ip);

    return 0;
error:
    printk("irelease(), called @ 0x%p, error=%d\n", return_address(0), err);
    return err;
}

static iops_t iops =
{
    0
};