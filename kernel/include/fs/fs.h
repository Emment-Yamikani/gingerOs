#ifndef FS_FS_H
#define FS_FS_H 1

#include <fs/dentry.h>
#include <lib/types.h>
#include <lib/stdint.h>
#include <locks/spinlock.h>
#include <locks/cond.h>
#include <ds/queue.h>
#include <fs/path.h>
#include <sys/_stat.h>
#include <sys/fcntl.h>

typedef enum
{
    FS_INV = 0,
    FS_RGL = 1,
    FS_DIR = 2,
    FS_BLKDEV = 3,
    FS_CHRDEV = 4,
    FS_PIPE = 5
} itype_t;

#define NFILE 16

#define FNAME_MAX 255

#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif
#ifndef SEEK_END
#define SEEK_END 2
#endif

struct filesystem;
struct super_block;
struct fops;

typedef struct iops
{
    int (*open)(inode_t *, int, ...);
    int (*creat)(inode_t *, dentry_t *, int);
    int (*find)(inode_t *, dentry_t *, inode_t **);
    int (*sync)(inode_t *);
    int (*close)(inode_t *);
    int (*ioctl)(inode_t *, int, void *);
    size_t (*read)(inode_t *, off_t, void *, size_t);
    size_t (*write)(inode_t *, off_t, void *, size_t);
    int (*lseek)(inode_t *, off_t, int);

} iops_t;

/*inode*/
typedef struct inode
{
    size_t i_size;      /*file size*/
    int i_rdev;         /*real device*/
    int i_mask;         /*access mask*/
    uid_t i_uid;        /*user(owner) ID*/
    gid_t i_gid;        /*group ID*/
    int i_refs;         /*reference count*/
    itype_t i_type;     /*file type*/
    spinlock_t *i_lock; /*per-inode lock*/
    cond_t *i_readers;    /*per-inode readers wait condition*/
    cond_t *i_writers;    /*per-inode writers wait condition*/
    void *i_priv;       /*private data*/
    struct filesystem *ifs; /*super block on which this inode resides*/
} inode_t;

/*is inode pointing to a device*/
#define ISDEV(inode) ((inode->i_type == FS_CHRDEV) || (inode->i_type == FS_BLKDEV))

typedef struct sops
{
    int (*sync)(super_block_t *);
    int (*write_super)(super_block_t *);
    int (*alloc_inode)(super_block_t *, inode_t **);
    int (*destroy_inode)(inode_t *);
    int (*read_inode)(inode_t *);
    int (*write_inode)(inode_t *);
} sops_t;

typedef struct uio
{
    char *u_cwd; /*working directory*/
    int u_uid;   /*real user identifier*/
    int u_gid;   /*group identifier*/
    int u_euid;  /*effective user identidier*/
    int u_egid;  /*effective group identifier*/
    int u_suid;  /*saved user identifier*/
    int u_sgid;  /*saved group identifier*/
} uio_t;

struct fops
{
    int (*sync)(file_t *);
    int (*close)(file_t *);
    int (*perm)(file_t *, int);
    int (*open)(file_t *, int, ...);
    off_t (*lseek)(file_t *, off_t, int);
    int (*ioctl)(file_t *, int, void *);
    size_t (*read)(file_t *, void *, size_t);
    size_t (*write)(file_t *, void *, size_t);
    int (*stat)(file_t *, struct stat *);
    
    size_t (*eof)(file_t *);
    size_t (*can_read)(struct file *file, size_t size);
    size_t (*can_write)(struct file *file, size_t size);
};

typedef struct super_block
{
    int s_count;
    int s_magic;
    dentry_t *s_droot;
    inode_t *s_iroot;
    dev_t *s_dev;
    int s_flags;
    size_t s_blocksz;
    size_t s_maxfilesz;
    void *s_private;
    spinlock_t *s_lock;
    sops_t sops;
    struct fops *fops;
    struct iops *iops;
} super_block_t;

typedef struct filesystem
{
    char *fname;
    int (*load)();
    int (*mount)();
    int (*unmount)();
    queue_node_t *flist_node;
    struct super_block *fsuper;
} filesystem_t;

int vfs_register(struct filesystem *);

/*each task accesses a file through this file description*/
typedef struct file
{
    spinlock_t *f_lock;
    dentry_t *f_dentry; /*file identifier*/
    int f_flags;        /*file flags used to open this this*/
    inode_t *f_inode;   /*file inode pointed to*/
    off_t f_pos;        /*file read-write position*/
    int f_ref;          /*file reference count*/
} file_t;

typedef struct file_table
{
    uio_t uio;
    spinlock_t *lock;
    file_t *file[NFILE];
} file_table_t;

void f_free_table(struct file_table *ftable);
int f_alloc_table(struct file_table **rft);
#define file_table_assert(ft) assert(ft, "no file_table")
#define file_table_assert_lock(tf) spin_assert_lock(ft->lock)

#define file_table_lock(table)    \
    {                             \
        file_table_assert(table); \
        spin_lock(table->lock);   \
    }

#define file_table_unlock(table)  \
    {                             \
        file_table_assert(table); \
        spin_unlock(table->lock); \
    }

int f_alloc_table(struct file_table **rft);

int vfs_init(void);

int vfs_mount_root(inode_t *root);

int vfs_bind(dentry_t *parent, dentry_t *child);

int vfs_get_mountpoint(char **abs_path, path_t **ref);

int vfs_parse_path(char *path, char *cwd, char **ref);

int vfs_mount(const char *dir, const char *name, inode_t *ip);

int vfs_open(const char *fn, uio_t *uio, int mode, inode_t **iref);

int vfs_lookup(const char *fn, uio_t *uio, int mode, inode_t **iref, dentry_t **dref);

/* inode helpers*/

/*alloc a new inode*/
int ialloc(inode_t **);

/*release an inode*/
int irelease(inode_t *);

#define iassert(ip) assert(ip, "no inode")

/*lock inode*/
#define ilock(ip) { iassert(ip); spin_lock(ip->i_lock);}

/*unlock inode*/
#define iunlock(ip) { iassert(ip); spin_unlock(ip->i_lock);}

int iincrement(inode_t *inode);

int iclose(inode_t *);
int iopen(inode_t *, int, ...);
int iioctl(inode_t *, int, void *);
int ifind(inode_t *, dentry_t *, inode_t **);
size_t iread(inode_t *, off_t, void *, size_t);
size_t iwrite(inode_t *, off_t, void *, size_t);
int iperm(inode_t *, uio_t *, int);
int isleek(inode_t *, off_t, int);
int istat(inode_t *, struct stat *buf);

/*** @brief file helpers*/

#define flock(f) spin_lock(f->f_lock)
#define funlock(f) spin_unlock(f->f_lock)

int fcan_read(struct file *file, size_t size);
int fcan_write(struct file *file, size_t size);
size_t feof(file_t *);
int fclose(file_t *);
int fdup(file_t *);
int fopen(file_t *, int, ...);
int fread(file_t *, void *, size_t);
int fwrite(file_t *, void *, size_t);
int ffstat(file_t *, struct stat *);
off_t flseek(file_t *, off_t, int);
int fioctl(file_t *, int, void * /* args */);
void file_free(file_t *file);

#endif // FS_FS_H