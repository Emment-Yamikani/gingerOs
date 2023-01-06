#ifndef FS_DENTRY_H
#define FS_DENTRY_H 1

#include <lib/types.h>
#include <locks/spinlock.h>

typedef struct dops
{
    int (*open)(dentry_t *);
    int (*close)(dentry_t *);
    int (*dup)(dentry_t *);
    inode_t *(*iget)(dentry_t *);
} dops_t;

typedef struct dentry
{
    char    *d_name;
    int     d_ref;
    void    *d_priv;
    inode_t *d_inode;
    dentry_t *d_prev;
    dentry_t *d_next;
    dentry_t *d_parent;
    dentry_t *d_children;
    spinlock_t *d_lock;
    dops_t   dops;
} dentry_t;


int dentry_open(dentry_t *);
int dentry_close(dentry_t *);
int dentry_dup(dentry_t *);
inode_t *dentry_iget(dentry_t *);


void dlock(dentry_t *dentry);
void dunlock(dentry_t *dentry);
int dentry_alloc(char *name, dentry_t **ref);


void dentry_dump(dentry_t *dentry);

#endif // FS_DENTRY_H