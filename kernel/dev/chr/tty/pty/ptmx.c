#include <dev/tty.h>
#include <dev/dev.h>
#include <fs/fs.h>
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
#include <dev/pty.h>

static dev_t ptmxdev;

int ptmx_probe()
{
    return 0;
}

int ptmx_mount(void)
{
    dev_attr_t attr = {
        .devid = *_DEVID(FS_CHRDEV, _DEV_T(DEV_PTMX, 0)),
        .size = 1,
        .mask = 0666,
    };
    return devfs_mount("ptmx", attr);
}

int ptmx_open(struct devid *dd __unused, int mode __unused, ...)
{
    return 0;
}

int ptmx_close(struct devid *dd __unused)
{
    return -EINVAL;
}

int ptmx_ioctl(struct devid *dd, int req, void *argp)
{
    spin_lock(ptylk);
    PTY pty = ptytable[dd->dev_minor];
    spin_unlock(ptylk);

    switch (req)
    {
    case TIOCGPTN:
        *(int *)argp = pty->id;
        return 0;
    case TIOCSPTLCK:
        *(int *)argp = 0; /* FIXME */
        return 0;
    }

    return tty_ioctl(pty->tty, req, argp);
}

static size_t ptmx_read(struct devid *dd, off_t offset __unused, void *buf, size_t size)
{
    ssize_t nbyte = 0;
    spin_lock(ptylk);
    struct pty *pty = ptytable[dd->dev_minor];
    spin_unlock(ptylk);

    /*ringbuf_lock(pty->out);
    ssize_t nbyte = ringbuf_read(pty->out, size, buf);
    ringbuf_unlock(pty->out);
    */

    nbyte = iread(pty->master_io.read, 0, buf, size);
    return nbyte;
}

static size_t ptmx_write(struct devid *dd, off_t offset __unused, void *buf, size_t size)
{
    spin_lock(ptylk);
    struct pty *pty = ptytable[dd->dev_minor];
    spin_unlock(ptylk);
    ssize_t nbyte = tty_master_write(pty->tty, size, buf);
    return nbyte;
}

int ptmx_init(void)
{
    return kdev_register(&ptmxdev, DEV_PTMX, FS_CHRDEV);
}

int ptmx_fopen(file_t *file __unused, int oflags __unused, ...)
{
    int err = 0;
    char *pts_name = NULL;
    inode_t *master = NULL;
    dentry_t *dentry = NULL;

    if ((err = pty_new(proc, &master, &pts_name)))
        goto error;

    if ((err = dentry_alloc(pts_name, &dentry)))
        goto error;

    file->f_dentry = dentry;
    file->f_inode = master;

    return 0;
error:
    klog(KLOG_FAIL, "failed to open ptmx file, error=%d\n", err);
    return err;
}

static struct dev ptmxdev = {
    .dev_name = "ptmx",
    .dev_probe = ptmx_probe,
    .dev_mount = ptmx_mount,
    .devid = _DEV_T(DEV_PTMX, 2),
    .devops = {
        .open = ptmx_open,
        .read = ptmx_read,
        .write = ptmx_write,
        .ioctl = ptmx_ioctl,
        .close = ptmx_close,
    },

    .fops = {
        .close = posix_file_close,
        .ioctl = posix_file_ioctl,
        .lseek = posix_file_lseek,
        .open = ptmx_fopen,
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

MODULE_INIT(pmtx, ptmx_init, NULL);