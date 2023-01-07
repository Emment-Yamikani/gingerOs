#include <printk.h>
#include <lib/string.h>
#include <fs/fs.h>
#include <lime/string.h>
#include <fs/pipefs.h>
#include <mm/kalloc.h>
#include <lib/string.h>
#include <lime/string.h>
#include <bits/errno.h>
#include <sys/thread.h>
#include <fs/posix.h>

static iops_t pipefs_iops;
static filesystem_t pipefs;
static struct super_block pipefs_sb;

int pipefs_init(void)
{
    return vfs_register(&pipefs);
}

static int pipefs_mkpipe(pipe_t **rpipe)
{
    int err = 0;
    cond_t *ww = NULL;
    cond_t *rw = NULL;
    pipe_t *pipe = NULL;
    ringbuf_t *ring = NULL;
    spinlock_t *lock = NULL;

    assert(rpipe, "no pipe reference");

    if ((err = cond_init(NULL, "readers", &rw)))
        goto error;
    
    if ((err = cond_init(NULL, "writers", &ww)))
        goto error;

    if ((err = ringbuf_new(PIPESZ, &ring)))
        goto error;

    if ((err = spinlock_init(NULL, "pipe", &lock)))
        goto error;

    if (!(pipe = kmalloc(sizeof *pipe)))
    {
        err = -ENOMEM;
        goto error;
    }

    *pipe = (pipe_t)
    {
        .read_open =0,
        .write_open =0,
        .ringbuf = ring,
        .readers = rw,
        .writers = ww,
        .lock = lock
    };

    *rpipe = pipe;
    return 0;
error:
    if (ww) cond_free(ww);
    if (rw) cond_free(rw);
    if (pipe) kfree(pipe);
    if (ring) ringbuf_free(ring);

    return err;
}

static void pipefs_free(pipe_t *pipe)
{
    assert(pipe, "no pipe");

    if (pipe->ringbuf) ringbuf_free(pipe->ringbuf);
    if (pipe->writers) cond_free(pipe->writers);
    if (pipe->readers) cond_free(pipe->readers);
    kfree(pipe);
}

int pipefs_pipe(file_t *f0, file_t *f1)
{
    int err = 0;
    pipe_t *pipe = NULL;
    inode_t *r = NULL, *w = NULL;
    assert(f0, "no file descriptor");
    assert(f1, "no file descriptor");

    if ((err = ialloc(&r)))
        goto error;

    if ((err = ialloc(&w)))
        goto error;

    if ((err = pipefs_mkpipe(&pipe)))
        goto error;

    r->i_mask = 0400;
    r->i_type = FS_PIPE;
    r->i_size = PIPESZ;
    r->ifs = &pipefs;
    r->i_priv = (void *)pipe;

    w->i_mask = 0200;
    w->i_type = FS_PIPE;
    w->i_size = PIPESZ;
    w->ifs = &pipefs;
    w->i_priv = (void *)pipe;
    
    pipe->read_open = 1;
    pipe->write_open =1;
    
    f0->f_inode = r;
    f1->f_inode = w;

    return 0;
error:
    if (r) irelease(r);
    if (w) irelease(w);
    if (pipe) pipefs_free(pipe);
    return err;
}

static int pipefs_close(pipe_t *pipe, int writable)
{
    assert(pipe, "no pipe");
    spin_lock(pipe->lock);
    if (writable)
    {
        pipe->write_open =0;
        cond_signal(pipe->readers);
    }
    else
    {
        pipe->read_open = 0;
        cond_signal(pipe->writers);
    }

    if (!pipe->read_open && !pipe->write_open)
    {
        klog(KLOG_OK, "free pipe\n");
        spin_unlock(pipe->lock);
        pipefs_free(pipe);
        return 0;
    }

    spin_unlock(pipe->lock);

    return 0;
}

static size_t pipe_iread(inode_t *inode, off_t off __unused, void *buf, size_t size)
{
    size_t read =0, pos =0;
    pipe_t *pipe = NULL;

    ilock(inode);
    pipe = inode->i_priv;

    spin_lock(pipe->lock);
    if ((pipe->read_open == 0) || ((inode->i_mask & S_IREAD) == 0)) // pipe not open for read
    {
        spin_unlock(pipe->lock);
        iunlock(inode);
        return 0;
    }

    while(read < size)
    {
        if (atomic_read(&current->t_killed) || pipe->write_open == 0) // in case we have been killed break
            break;
        
        ringbuf_lock(pipe->ringbuf);
        int cursz = ringbuf_available(pipe->ringbuf);
        ringbuf_unlock(pipe->ringbuf);
        
        if (cursz == 0) // pipe is empty, sleep
        {
            cond_signal(pipe->writers);
            spin_unlock(pipe->lock);
            iunlock(inode);
            cond_wait(pipe->readers);
            ilock(inode);
            spin_lock(pipe->lock);
        }

        pos += read += ringbuf_read(pipe->ringbuf, (size - read), (buf + pos));
        cond_signal(pipe->writers); //notify the writers that we are done for now
    }
    spin_unlock(pipe->lock);
    iunlock(inode);

    return read;
}

static size_t pipe_iwrite(inode_t *inode, off_t off __unused, void *buf, size_t size)
{
    size_t written =0, pos = 0;
    pipe_t *pipe = NULL;

    ilock(inode);
    pipe = inode->i_priv;

    spin_lock(pipe->lock);
    if ((pipe->write_open == 0) || ((inode->i_mask & S_IWRITE) == 0)) // pipe not open for writing
    {
        spin_unlock(pipe->lock);
        iunlock(inode);
        return 0;
    }

    while (written < size)
    {
        if (atomic_read(&current->t_killed) || pipe->read_open == 0) // in case we have been killed break
            break;
        
        ringbuf_lock(pipe->ringbuf);
        int cursz = ringbuf_available(pipe->ringbuf);
        ringbuf_unlock(pipe->ringbuf);
        
        if (cursz == PIPESZ) // pipe is full, sleep
        {
            cond_signal(pipe->readers);
            spin_unlock(pipe->lock);
            iunlock(inode);
            cond_wait(pipe->writers);
            ilock(inode);
            spin_lock(pipe->lock);
        }
        pos += written += ringbuf_write(pipe->ringbuf, (size - written), buf + pos);
        cond_signal(pipe->readers); // notify the writers that we are done for now
    }
    spin_unlock(pipe->lock);
    iunlock(inode);

    return written;
}

static int pipe_icreat(inode_t *inode __unused, dentry_t *dentry __unused, int mode __unused)
{
    return -EINVAL;
}

static int pipe_ifind(inode_t *inode __unused, dentry_t *dentry __unused, inode_t **ref __unused)
{
    return -EINVAL;
}

static int pipe_isync(inode_t *inode __unused)
{
    return -EINVAL;
}

static int pipe_iopen(inode_t *inode __unused, int oflags __unused, ...)
{
    return -EINVAL;
}

static int pipe_iioctl(inode_t *inode __unused, int req __unused, void *p __unused)
{
    return -EINVAL;
}

static int pipe_ilseek(inode_t *inode __unused, off_t off __unused, int whence __unused)
{
    return -EINVAL;
}

static int pipefs_iclose(inode_t *inode __unused)
{
    int writable = 0;
    pipe_t *pipe = NULL;
    ilock(inode);
    pipe = (pipe_t *)inode->i_priv;
    writable = inode->i_mask & 0200;
    iunlock(inode);
    return pipefs_close(pipe, writable);
}

static size_t pipefs_can_read(struct file *file, size_t size)
{
    struct inode *node = file->f_inode;
    struct pipe *pipe = node->i_priv;
    ringbuf_lock(pipe->ringbuf);
    size_t can = size <= ringbuf_available(pipe->ringbuf);
    ringbuf_unlock(pipe->ringbuf);
    return can;
}

static size_t pipefs_can_write(struct file *file, size_t size)
{
    struct inode *node = file->f_inode;
    struct pipe *pipe = node->i_priv;
    ringbuf_lock(pipe->ringbuf);
    size_t can = size >= pipe->ringbuf->size - ringbuf_available(pipe->ringbuf);
    ringbuf_unlock(pipe->ringbuf);
    return can;
}

int pipefs_mount()
{
    int err = 0;
    inode_t *pipefs_iroot = NULL;

    if ((err = ialloc(&pipefs_iroot)))
        goto error;
    
    pipefs_iroot->i_mask = 0770;
    pipefs_iroot->i_type = FS_DIR;
    pipefs_sb.s_count = 1;
    pipefs_sb.s_iroot = pipefs_iroot;
    
    if ((err = vfs_mount("/", "pipefs", pipefs_iroot)))
        goto error;
    
    return 0;
error:
    if (pipefs_iroot)
        irelease(pipefs_iroot);
    klog(KLOG_FAIL, "Failed to mount pipefs, error=%d\n", err);
    return err;
}

int pipefs_load()
{
    pipefs_sb.fops->can_read = pipefs_can_read;
    pipefs_sb.fops->can_write = pipefs_can_write;
    pipefs_sb.fops->eof = (size_t(*)(struct file *))__never;
    return 0;
}

static iops_t pipefs_iops=
{
    .close = pipefs_iclose,
    .read = pipe_iread,
    .write = pipe_iwrite,
    .creat = pipe_icreat,
    .find = pipe_ifind,
    .ioctl = pipe_iioctl,
    .lseek = pipe_ilseek,
    .open = pipe_iopen,
    .sync = pipe_isync
};

static super_block_t pipefs_sb = {
    .fops = &posix_fops,
    .iops = &pipefs_iops,
    .s_blocksz = PIPESZ,
    .s_magic = 0xc0de,
    .s_maxfilesz = PIPESZ,
};

static filesystem_t pipefs = {
    .fname = "pipefs",
    .flist_node = NULL,
    .fsuper = &pipefs_sb,
    .load = pipefs_load,
    .mount = pipefs_mount,
};