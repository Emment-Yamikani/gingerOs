#include <fs/fs.h>
#include <dev/dev.h>
#include <printk.h>
#include <bits/errno.h>
#include <lib/string.h>

int kdev_fcan_read(struct devid *dd, file_t *file, size_t sz)
{
    dev_t *dev = NULL;
    if ((dev = kdev_get(dd)) == NULL)
        return -ENXIO;
    if (!dev->fops.can_read)
        return -ENXIO;
    return dev->fops.can_read(file, sz);
}

int kdev_fcan_write(struct devid *dd, file_t *file, size_t sz)
{
    dev_t *dev = NULL;
    if ((dev = kdev_get(dd)) == NULL)
        return -ENXIO;
    if (!dev->fops.can_write)
        return -ENXIO;
    return dev->fops.can_write(file, sz);
}

size_t kdev_feof(struct devid *dd, file_t *file)
{
    dev_t *dev = NULL;
    if ((dev = kdev_get(dd)) == NULL)
        return -ENXIO;
    if (!dev->fops.eof)
        return -ENXIO;
    return dev->fops.eof(file);
}

size_t kdev_fread(struct devid *dd, file_t *file, void *buf, size_t sz)
{
    dev_t *dev = NULL;
    if ((dev = kdev_get(dd)) == NULL)
        return -ENXIO;
    if (!dev->fops.read)
        return -ENXIO;
    return dev->fops.read(file, buf, sz);
}

size_t kdev_fwrite(struct devid *dd, file_t *file, void *buf, size_t sz)
{
    dev_t *dev = NULL;
    if ((dev = kdev_get(dd)) == NULL)
        return -ENXIO;
    if (!dev->fops.write)
        return -ENXIO;
    return dev->fops.write(file, buf, sz);
}

off_t kdev_flseek(struct devid *dd, file_t *file, off_t offset, int whence)
{
    dev_t *dev = NULL;
    if ((dev = kdev_get(dd)) == NULL)
        return -ENXIO;
    if (!dev->fops.lseek)
        return -ENXIO;
    return dev->fops.lseek(file, offset, whence);
}

int kdev_fsync(struct devid *dd, file_t *file)
{
    dev_t *dev = NULL;
    if ((dev = kdev_get(dd)) == NULL)
        return -ENXIO;
    if (!dev->fops.sync)
        return -ENXIO;
    return dev->fops.sync(file);
}

off_t kdev_fclose(struct devid *dd, file_t *file)
{
    dev_t *dev = NULL;
    if ((dev = kdev_get(dd)) == NULL)
        return -ENXIO;
    if (!dev->fops.close)
        return -ENXIO;
    return dev->fops.close(file);
}

off_t kdev_fperm(struct devid *dd, file_t *file, int oflags)
{
    dev_t *dev = NULL;
    if ((dev = kdev_get(dd)) == NULL)
        return -ENXIO;
    if (!dev->fops.perm)
        return -ENXIO;
    return dev->fops.perm(file, oflags);
}

off_t kdev_fopen(struct devid *dd, file_t *file, int oflags, ...)
{
    dev_t *dev = NULL;
    if ((dev = kdev_get(dd)) == NULL)
        return -ENXIO;
    if (!dev->fops.open)
        return -ENXIO;
    return dev->fops.open(file, oflags);
}

int kdev_fioctl(struct devid *dd, file_t *file, int request, void *args)
{
    dev_t *dev = NULL;
    if ((dev = kdev_get(dd)) == NULL)
        return -ENXIO;
    if (!dev->fops.ioctl)
        return -ENXIO;
    return dev->fops.ioctl(file, request, args);
}

int kdev_fstat(struct devid *dd, file_t *file, struct stat *buf)
{
    dev_t *dev = NULL;
    if ((dev = kdev_get(dd)) == NULL)
        return -ENXIO;
    if (!dev->fops.stat)
        return -ENXIO;
    return dev->fops.stat(file, buf);
}

int kdev_fmmap(struct devid *dd, file_t *file, vmr_t *vmr)
{
    dev_t *dev = NULL;
    if ((dev = kdev_get(dd)) == NULL)
        return -ENXIO;
    if (!dev->fops.mmap)
        return -ENXIO;
    return dev->fops.mmap(file, vmr);
}