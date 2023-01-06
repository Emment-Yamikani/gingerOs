#include <fs/fs.h>
#include <fs/dentry.h>
#include <bits/errno.h>
#include <lib/string.h>
#include <mm/kalloc.h>
#include <lime/string.h>
#include <printk.h>

static dops_t dops;

int dentry_alloc(char *name, dentry_t **ref)
{
    int err =0;
    char *tmp_name = NULL;
    dentry_t *dentry = NULL;
    spinlock_t *lock = NULL;

    if (!name || !ref)
        return -EINVAL;

    if (!(dentry = __cast_to_type(dentry)kmalloc(sizeof *dentry)))
    {
        err = -ENOMEM;
        goto error;
    }

    tmp_name = combine_strings(name, "-dentry");
    if ((err = spinlock_init(NULL, tmp_name, &lock)))
        goto error;

    memset(dentry, 0, sizeof *dentry);

    if (!(tmp_name = strdup(name)))
    {
        err = -ENOMEM;
        goto error;
    }

    dentry->d_lock = lock;
    dentry->d_name = tmp_name;
    dentry->d_ref  = 1;
    dentry->dops = dops;
    
    *ref = dentry;
    return 0;

error:
    if (dentry)
        kfree(dentry);
    if (tmp_name)
        kfree(tmp_name);
    if (lock)
        spinlock_free(lock);
    printk("dentry_alloc(): called @ 0x%p, error=%d\n", return_address(0), err);
    return err;
}

static int dentry_free(dentry_t *dentry)
{
    if (!dentry)
        return -EINVAL;
    
    dlock(dentry);
    
    if (dentry->d_prev)
    {
        dlock(dentry->d_prev);
        dentry->d_prev->d_next = dentry->d_next;
        dunlock(dentry->d_prev);
    }

    if (dentry->d_next)
    {
        dlock(dentry->d_next);
        dentry->d_next->d_prev = dentry->d_prev;
        dunlock(dentry->d_next);
    }
    
    dunlock(dentry);

    printk("dentry_free(%s)\n", dentry->d_name);

    if (dentry->d_name)
    {
        kfree(dentry->d_name);
    }
    if (dentry->d_lock)
        spinlock_free(dentry->d_lock);
    *dentry = (dentry_t){0};
    kfree(dentry);
    return 0;
}

void dlock(dentry_t *dentry)
{
    if (!dentry)
        panic("dlock(): called @ 0x%p, no dentry to lock\n", return_address(0));
    spin_lock(dentry->d_lock);
}

void dunlock(dentry_t *dentry)
{
    if (!dentry)
        panic("dunlock(): called @ 0x%p, no dentry to unlock\n", return_address(0));
    spin_unlock(dentry->d_lock);
}

int dentry_open(dentry_t *dentry __unused)
{
    printk("dentry_open\n");
    return -EINVAL;
}

int dentry_close(dentry_t *dentry)
{
    if (!dentry)
        return -EINVAL;
    if (!--dentry->d_ref)
    {
        iclose(dentry->d_inode);
        dentry_free(dentry);
    }
    return 0;
}

int dentry_dup(dentry_t *dentry)
{
    if (!dentry)
        return -EINVAL;
    dlock(dentry);
    ++dentry->d_ref;
    dunlock(dentry);
    return 0;
}

inode_t *dentry_iget(dentry_t *dentry)
{
    inode_t *inode = NULL;
    if (!dentry)
        return NULL;
    dlock(dentry);
    inode = dentry->d_inode;
    dunlock(dentry);
    return inode;
}

void dentry_dump(dentry_t *dentry)
{
    if (!dentry)
        return;

    dlock(dentry);
    printk("\e[017;0mdentry info\e[0m\n");
    printk("name:       \e[0;012m%s\e[0m\n"
           "ref:        \e[0;012m%4d\e[0m\n"
           "lock:       \e[0;012m%p\e[0m\n"
           "inode:      \e[0;012m%p\e[0m\n"
           "priv:       \e[0;012m%p\e[0m\n"
           "prev:       \e[0;012m%p\e[0m\n"
           "next:       \e[0;012m%p\e[0m\n"
           "parent:     \e[0;012m%p\e[0m\n",
           dentry->d_name ? dentry->d_name : "(null)",
           dentry->d_ref,
           dentry->d_lock,
           dentry->d_inode,
           dentry->d_priv,
           dentry->d_prev,
           dentry->d_next,
           dentry->d_parent);
    dunlock(dentry);
}

static dops_t dops = 
{
    .open = dentry_open,
    .close  = dentry_close,
    .dup = dentry_dup,
    .iget = dentry_iget
};