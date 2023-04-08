#include <bits/errno.h>
#include <fs/fs.h>
#include <lib/string.h>
#include <lime/string.h>
#include <mm/kalloc.h>
#include <printk.h>

int ialloc(inode_t **ref)
{
    int err =0;
    inode_t *ip = NULL;
    spinlock_t *lock = NULL;
    mapping_t *mapping = NULL;
    //cond_t *reader_wait = NULL, *writer_wait = NULL;

    if (!ref)
        return -EINVAL;
    
    if (!(ip = __cast_to_type(ip)kmalloc(sizeof *ip)))
    {
        err = -ENOMEM;
        goto error;
    }

    if ((err = spinlock_init(NULL, "inode", &lock)))
        goto error;

    if ((err = mapping_new(&mapping)))
        goto error;

    /*
    if ((err = cond_init(NULL, "inode-reader", &reader_wait)))
        goto error;
    if ((err = cond_init(NULL, "inode-writer", &writer_wait)))
        goto error;
    */

    memset(ip, 0, sizeof *ip);

    ip->i_refs = 1;
    ip->i_lock = lock;
    ip->mapping = mapping;
    mapping->inode = ip;
    ip->i_refs++;
    /*
    ip->i_readers = reader_wait;
    ip->i_writers = writer_wait;
    */

    *ref = ip;
    return 0;
error:
    if (ip) kfree(ip);
    if (lock) spinlock_free(lock);
    if (mapping) mapping_free(mapping);

    /*
    if (reader_wait)
        cond_free(reader_wait);
    if (writer_wait)
        cond_free(writer_wait);
    */
    printk("ialloc(): called @ 0x%p, error=%d\n", return_address(0), err);
    return err;
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
    cond_free(ip->i_readers);
    cond_free(ip->i_writers);
    kfree(ip);

    return 0;
error:
    printk("irelease(), called @ 0x%p, error=%d\n", return_address(0), err);
    return err;
}