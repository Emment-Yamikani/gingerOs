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
#include <sys/thread.h>
#include <sys/proc.h>
#include <arch/i386/cpu.h>

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
    pipe_t *pipe = NULL;
    ringbuf_t *ring = NULL;
    spinlock_t *lock = NULL;
    cond_t *readers = NULL, *writers = NULL;

    assert(rpipe, "no pipe reference");

    if ((err = cond_init(NULL, "pipe_readers", &readers)))
        goto error;

    if ((err = cond_init(NULL, "pipe_writers", &writers)))
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

    *pipe = (pipe_t){
        .ropen = 0,
        .wopen = 0,
        .ringbuf = ring,
        .lock = lock,
        .readers = readers,
        .writers = writers,
    };

    *rpipe = pipe;
    return 0;
error:
    if (readers)
        cond_free(readers);
    if (writers)
        cond_free(writers);
    if (pipe)
        kfree(pipe);
    if (ring)
        ringbuf_free(ring);
    return err;
}

static void pipefs_free(pipe_t *pipe)
{
    assert(pipe, "no pipe");

    if (pipe->ringbuf)
        ringbuf_free(pipe->ringbuf);
    if (pipe->lock)
        spinlock_free(pipe->lock);
    kfree(pipe);
}

int pipefs_pipe_raw(inode_t **read, inode_t **write)
{
    int err = 0;
    pipe_t *pipe = NULL;
    inode_t *iread = NULL, *iwrite = NULL;
    assert(read, "no file descriptor");
    assert(write, "no file descriptor");

    if ((err = ialloc(&iread)))
        goto error;

    if ((err = ialloc(&iwrite)))
        goto error;

    if ((err = pipefs_mkpipe(&pipe)))
        goto error;

    iread->i_mask = 0444;
    iread->i_priv = pipe;
    iread->ifs = &pipefs;

    iwrite->i_mask = 0222;
    iwrite->i_priv = pipe;
    iwrite->ifs = &pipefs;

    pipe->ropen = 1;
    pipe->wopen = 1;

    *read = iread;
    *write = iwrite;

    return 0;
error:
    if (iread)
        irelease(iread);
    if (iwrite)
        irelease(iwrite);
    if (pipe)
        pipefs_free(pipe);
    return err;
}

int pipefs_pipe(file_t *read, file_t *write)
{
    int err = 0;
    pipe_t *pipe = NULL;
    inode_t *iread = NULL, *iwrite = NULL;
    assert(read, "no file descriptor");
    assert(write, "no file descriptor");

    if ((err = ialloc(&iread)))
        goto error;

    if ((err = ialloc(&iwrite)))
        goto error;

    if ((err = pipefs_mkpipe(&pipe)))
        goto error;

    iread->i_mask = 0444;
    iread->i_priv = pipe;
    iread->ifs = &pipefs;
    cond_free(iread->i_readers);
    iread->i_readers = NULL;
    cond_free(iread->i_writers);
    iread->i_writers = NULL;

    iwrite->i_mask = 0222;
    iwrite->i_priv = pipe;
    iwrite->ifs = &pipefs;
    cond_free(iwrite->i_readers);
    iwrite->i_readers = NULL;
    cond_free(iwrite->i_writers);
    iwrite->i_writers = NULL;

    pipe->ropen = 1;
    pipe->wopen = 1;

    read->f_inode = iread;
    read->f_flags |= O_RDONLY;

    write->f_inode = iwrite;
    write->f_flags |= O_WRONLY;

    return 0;
error:
    if (iread)
        irelease(iread);
    if (iwrite)
        irelease(iwrite);
    if (pipe)
        pipefs_free(pipe);
    return err;
}

static int pipefs_close(pipe_t *pipe, int writable)
{
    assert(pipe, "no pipe");
    spin_lock(pipe->lock);

    if (writable)
    {
        //klog(KLOG_OK, "write pipe end\n");
        pipe->wopen = 0;
    }
    else
    {
        //klog(KLOG_OK, "read pipe end\n");
        pipe->ropen = 0;
    }

    if (!pipe->ropen && !pipe->wopen)
    {
        //klog(KLOG_OK, "free pipe\n");
        spin_unlock(pipe->lock);
        pipefs_free(pipe);
        return 0;
    }

    spin_unlock(pipe->lock);
    return 0;
}

static size_t pipe_iread(inode_t *inode, off_t off __unused, void *buf __unused, size_t size)
{
    size_t read = 0, pos __unused = 0;
    pipe_t *pipe = NULL;

    ilock(inode);
    pipe = inode->i_priv;
    spin_lock(pipe->lock);

    if ((inode->i_mask & S_IREAD) == 0) // pipe not open for read
    {
        printk("pipe not open for reading\n");
        spin_unlock(pipe->lock);
        iunlock(inode);
        return -1;
    }

    //printk("pipe read\n");

    while (read < size)
    {
        ringbuf_lock(pipe->ringbuf);
        if (ringbuf_isempty(pipe->ringbuf)) //pipe is empty
        {
            //printk("pipe is empty\n");
            if (pipe->wopen == 0) // write end of pipe is closed
            {
                //printk("broken pipe, no write end\n");
                ringbuf_unlock(pipe->ringbuf);
                spin_unlock(pipe->lock);
                iunlock(inode);
                if (!read)
                    return -EPIPE;
                else
                    return read;
            }

            cond_signal(pipe->writers);
            
            ringbuf_unlock(pipe->ringbuf);
            spin_unlock(pipe->lock);
            iunlock(inode);
            
            cond_wait(pipe->readers);
            
            ilock(inode);
            spin_lock(pipe->lock);
            ringbuf_lock(pipe->ringbuf);
        }

        read += ringbuf_read(pipe->ringbuf, size - read, buf + read);
        ringbuf_unlock(pipe->ringbuf);
    }

    cond_signal(pipe->writers);
    spin_unlock(pipe->lock);
    iunlock(inode);
    return read;
}

static size_t pipe_iwrite(inode_t *inode, off_t off __unused, void *buf __unused, size_t size)
{
    size_t written = 0, pos __unused = 0;
    pipe_t *pipe = NULL;

    ilock(inode);
    pipe = inode->i_priv;

    spin_lock(pipe->lock);
    if ((inode->i_mask & S_IWRITE) == 0) // pipe not open for writing
    {
        printk("pipe not open for writing\n");
        spin_unlock(pipe->lock);
        iunlock(inode);
        return -1;
    }

    //printk("pipe write\n");

    while (written < size)
    {
        ringbuf_lock(pipe->ringbuf);
        if (ringbuf_available(pipe->ringbuf) == PIPESZ)
        {
            //printk("pipe is full\n");
            if (pipe->ropen == 0) // read end of pipe is closed
            {
                //printk("broken pipe, no read end\n");
                ringbuf_unlock(pipe->ringbuf);
                spin_unlock(pipe->lock);
                iunlock(inode);
                if (!written)
                    return -EPIPE;
                else return written;
            }

            cond_signal(pipe->readers);
            
            ringbuf_unlock(pipe->ringbuf);
            spin_unlock(pipe->lock);
            iunlock(inode);
            
            cond_wait(pipe->writers);

            ilock(inode);
            spin_lock(pipe->lock);
            ringbuf_lock(pipe->ringbuf);
        }

        written += ringbuf_write(pipe->ringbuf, size - written, buf + written);
        ringbuf_unlock(pipe->ringbuf);
    }
    
    cond_signal(pipe->readers);
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
    int err = 0, writable = 0;
    pipe_t *pipe = NULL;
    ilock(inode);
    pipe = (pipe_t *)inode->i_priv;
    writable = inode->i_mask & S_IWUSR ? 1 : 
               inode->i_mask & S_IWGRP ? 1 :
               inode->i_mask & S_IWOTH ? 1 : 0;
    err = pipefs_close(pipe, writable);
    iunlock(inode);
    return err;
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

static iops_t pipefs_iops = {
    .close = pipefs_iclose,
    .read = pipe_iread,
    .write = pipe_iwrite,
    .creat = pipe_icreat,
    .find = pipe_ifind,
    .ioctl = pipe_iioctl,
    .lseek = pipe_ilseek,
    .open = pipe_iopen,
    .sync = pipe_isync,
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