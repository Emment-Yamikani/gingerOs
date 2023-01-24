#include <dev/pty.h>
#include <dev/dev.h>
#include <fs/pipefs.h>
#include <mm/kalloc.h>
#include <lib/string.h>
#include <lime/string.h>
#include <bits/errno.h>
#include <arch/i386/cpu.h>
#include <sys/proc.h>
#include <sys/thread.h>
#include <sys/sched.h>
#include <printk.h>
#include <fs/posix.h>
#include <fs/devfs.h>
#include <lime/module.h>

static dev_t ptsdev;
PTY ptytable[256] = {0};
spinlock_t *ptylk = SPINLOCK_NEW("ptylock");

/* TTY Interface */
ssize_t pty_master_write(struct tty *tty, char *buf, long size)
{
    PTY pty = (PTY)tty->p;
    ssize_t nbyte = 0;
    if (pty->slave_unlocked == 0)
        return -EINVAL;
    /*ringbuf_lock(pty->out);
    ssize_t nbyte = ringbuf_write(pty->out, size, buf);
    ringbuf_unlock(pty->out);
    */

    nbyte = iwrite(pty->slave_io.write, 0, buf, size);
    return nbyte;
}

ssize_t pty_slave_write(struct tty *tty, char *buf, long size)
{
    PTY pty = (PTY)tty->p;
    ssize_t nbyte = 0;
    if (pty->slave_unlocked == 0)
        return -EINVAL;
    /*ringbuf_lock(pty->in);
    ssize_t nbyte = ringbuf_write(pty->in, size, buf);
    printk("pty_slave_write(%c)\n", *(char *)buf);
    ringbuf_unlock(pty->in);
    */

    nbyte = iwrite(pty->master_io.write, 0, buf, size);
    return nbyte;
}

int pty_getid(void)
{
    static atomic_t id = 0;
    spin_assert_lock(ptylk);
    if (atomic_read(&id) >= NELEM(ptytable))
        return -EAGAIN;
    return (int)atomic_incr(&id);
}

int ptm_new(PTY pty)
{
    int err = 0;
    inode_t *inode = NULL;

    pty_assert_lock(pty);

    if ((err = ialloc(&inode)))
        goto error;

    inode->i_priv = pty;
    inode->i_mask = 0777;
    inode->i_type = FS_CHRDEV;
    inode->i_size = 1;
    inode->i_rdev = _DEV_T(DEV_PTMX, pty->id);

    cond_free(inode->i_readers);
    cond_free(inode->i_writers);
    inode->i_readers = NULL;
    inode->i_writers = NULL;

    pty->master = inode;

    return 0;
error:
    if (inode)
        irelease(inode);
    klog(KLOG_FAIL, "failed to allocate master side of PTY, error=%d\n");
    return err;
}

int pts_new(PTY pty)
{
    int err = 0;
    char name[12];
    inode_t *inode = NULL;

    pty_assert_lock(pty);

    if ((err = ialloc(&inode)))
        goto error;

    inode->i_priv = pty;
    inode->i_mask = 0000; // use grant to allow access
    inode->i_rdev = _DEV_T(DEV_PTS, pty->id % NELEM(ptytable));
    inode->i_type = FS_CHRDEV;
    inode->i_size = 1;
    
    cond_free(inode->i_readers);
    cond_free(inode->i_writers);
    inode->i_readers = NULL;
    inode->i_writers = NULL;

    pty->slave = inode;

    snprintf(name, sizeof name, "%d\0", pty->id);

    if ((err = vfs_mount("/dev/pts", name, inode)))
        goto error;

    return 0;
error:
    if (inode)
        irelease(inode);
    klog(KLOG_FAIL, "failed to allocate slave side of PTY, erro=%d\n", err);
    return err;
}

int pty_new(proc_t *process, inode_t **master, char **ref)
{
    char name[12];
    PTY pty = NULL;
    int err = 0, id = 0;
    spinlock_t *lock = NULL;
    // ringbuf_t *in = NULL, *out = NULL;
    inode_t *master_read = NULL, *slave_read = NULL;
    inode_t *master_write = NULL, *slave_write = NULL;

    if ((err = spinlock_init(NULL, "pty", &lock)))
        goto error;

    /*if ((err = ringbuf_new(TTY_BUF_SIZE, &in)))
        goto error;

    if ((err = ringbuf_new(TTY_BUF_SIZE, &out)))
        goto error;
    */

    if ((err = pipefs_pipe_raw(&master_read, &slave_write)))
        goto error;

    if ((err = pipefs_pipe_raw(&slave_read, &master_write)))
        goto error;

    if ((pty = kmalloc(sizeof *pty)) == NULL)
    {
        err = -ENOMEM;
        goto error;
    }

    spin_lock(ptylk);

    if ((id = pty_getid()) < 0)
    {
        spin_unlock(ptylk);
        err = id;
        goto error;
    }

    memset(pty, 0, sizeof *pty);

    pty->id = id;
    // pty->in = in;
    // pty->out = out;
    pty->lock = lock;
    pty->master_io.read = master_read;
    pty->master_io.write = master_write;

    pty->slave_io.read = slave_read;
    pty->slave_io.write = slave_write;

    pty_lock(pty);

    if ((err = tty_new(process, TTY_BUF_SIZE, pty_master_write, pty_slave_write, pty, &pty->tty)))
    {
        spin_unlock(ptylk);
        goto error;
    }

    if ((err = ptm_new(pty)))
    {
        spin_unlock(ptylk);
        goto error;
    }

    if ((err = pts_new(pty)))
    {
        spin_unlock(ptylk);
        goto error;
    }

    memset(name, 0, sizeof name);
    snprintf(name, sizeof name, "%d\0", pty->id);

    if ((*ref = strdup(name)) == NULL)
    {
        err = -ENOMEM;
        spin_unlock(ptylk);
        goto error;
    }

    ptytable[pty->id] = pty;

    *master = pty->master;

    pty_unlock(pty);
    spin_unlock(ptylk);
    return 0;

error:
    if (pty)
    {
        if (pty->master)
            irelease(pty->master);

        if (pty->slave)
            irelease(pty->slave); // TODO: remove slave from /dev/pts/

        if (pty->tty)
            tty_free(pty->tty);

        kfree(pty);
    }

    /*if (in)
        ringbuf_free(in);

    if (out)
        ringbuf_free(out);
    */

    if (master_read)
        iclose(master_read);
    if (master_write)
        iclose(master_write);

    if (slave_read)
        iclose(slave_read);
    if (slave_write)
        iclose(slave_write);

    if (lock)
        spinlock_free(lock);

    klog(KLOG_FAIL, "failed to create new pty, error=%d\n", err);
    return err;
}

int pts_probe()
{
    return 0;
}

int pts_mount(void)
{
    int err = 0;
    inode_t *ptsdir = NULL;

    if ((err = ialloc(&ptsdir)))
        goto error;

    ptsdir->i_type = FS_DIR;
    ptsdir->i_mask = 0755;

    if ((err = vfs_mount("/dev", "pts", ptsdir)))
        goto error;

    return 0;
error:
    return err;
}

int pts_open(struct devid *dd __unused, int mode __unused, ...)
{
    return 0;
}

int pts_close(struct devid *dd __unused)
{
    return -EINVAL;
}

int pts_ioctl(struct devid *dd __unused, int req __unused, void *argp __unused)
{
    return -EINVAL;
}

static size_t pts_read(struct devid *dd, off_t offset __unused, void *buf, size_t size)
{
    ssize_t nbyte = 0;
    spin_lock(ptylk);
    PTY pty = ptytable[dd->dev_minor];
    if (pty->slave_unlocked == 0)
    {
        spin_unlock(ptylk);
        return -EINVAL;
    }
    spin_unlock(ptylk);

    /*ringbuf_lock(pty->in);
    ssize_t nbyte = ringbuf_read(pty->in, size, buf);
    printk("pts read: %d bytes\n", nbyte);
    ringbuf_unlock(pty->in);
    */

    nbyte = iread(pty->slave_io.read, 0, buf, size);
    return nbyte;
}

static size_t pts_write(struct devid *dd, off_t offset __unused, void *buf, size_t size)
{
    ssize_t nbyte = 0;
    spin_lock(ptylk);
    PTY pty = ptytable[dd->dev_minor];
    if (pty->slave_unlocked == 0)
    {
        spin_unlock(ptylk);
        return -EINVAL;
    }
    spin_unlock(ptylk);
    nbyte = tty_slave_write(pty->tty, size, buf);
    return nbyte;
}

int pts_init(void)
{
    return kdev_register(&ptsdev, DEV_PTS, FS_CHRDEV);
}

int pts_fopen(file_t *file __unused, int oflags __unused, ...)
{
    return 0;
}

static struct dev ptsdev = {
    .dev_name = "pts",
    .dev_probe = pts_probe,
    .dev_mount = pts_mount,
    .devid = _DEV_T(DEV_PTS, 0),
    .devops = {
        .open = pts_open,
        .read = pts_read,
        .write = pts_write,
        .ioctl = pts_ioctl,
        .close = pts_close,
    },

    .fops = {
        .open = pts_fopen,
        .close = posix_file_close,
        .ioctl = posix_file_ioctl,
        .lseek = posix_file_lseek,
        .perm = NULL,
        .read = posix_file_read,
        .sync = NULL,
        .stat = posix_file_ffstat,
        .write = posix_file_write,

        .can_read = (size_t(*)(struct file *, size_t))__always,
        .can_write = (size_t(*)(struct file *, size_t))__always,
        .eof = (size_t(*)(struct file *))__never,
    },
};

MODULE_INIT(pts, pts_init, NULL);