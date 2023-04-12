#include <mm/mmap.h>
#include <mm/kalloc.h>
#include <mm/usermap.h>
#include <bits/errno.h>


usermap_t *alloc_usermap(void)
{
    usermap_t *usr = NULL;
    if (!(usr = kcalloc(1, sizeof *usr)))
        return NULL;
    return usr;
}

void free_usermap(usermap_t *usermap)
{
    if (!usermap)
        return;
    *usermap = (usermap_t){0};
    kfree(usermap);
}

int shared_mapping_alloc(mmap_t *mmap, inode_t *inode, ssize_t pageno, uintptr_t virtaddr, shared_mapping_t **pshared_mapping) {
    int err = 0;
    spinlock_t *lock = NULL;
    shared_mapping_t *shared = NULL;

    if (!mmap || !inode || !virtaddr || pshared_mapping)
        return -EINVAL;

    if ((err = spinlock_init(NULL, "shared_mapping", &lock)))
        goto error;

    err = -ENOMEM;
    if ((shared = kcalloc(1, sizeof *shared)))
        goto error;
    
    spin_lock(lock);

    shared->refcnt = 1;
    shared->lock = lock;
    shared->mmap = mmap;
    shared->inode = inode;
    shared->pageno = pageno;
    shared->vaddr = virtaddr;

    *pshared_mapping = shared;
    return 0;
error:
    return err;
}