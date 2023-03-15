#include <fs/fs.h>
#include <lib/string.h>
#include <lime/module.h>
#include <mm/kalloc.h>
#include <printk.h>
#include <dev/fb.h>
#include <fs/posix.h>
#include <bits/errno.h>
#include <fs/devfs.h>
#include <arch/boot/boot.h>
#include <arch/i386/paging.h>
#include <mm/pmm.h>

static struct dev fbdev;
fb_fixinfo_t fix_info;
fb_varinfo_t var_info;
framebuffer_t framebuffer[NFBDEV];

int fbdev_probe()
{
    int err = 0;
    size_t fblen = 0;
    spinlock_t *lock = NULL;

    if (bootinfo.framebuffer.framebuffer_type != 1)
    {
        klog(KLOG_WARN, "linear framebuffer not found\n");
        return 0;
    }

    if ((err = spinlock_init(NULL, "fbdev0", &lock)))
        return err;

    fblen = bootinfo.framebuffer.framebuffer_pitch * bootinfo.framebuffer.framebuffer_height;

    frames_lock();
    int frame = bootinfo.framebuffer.framebuffer_addr / PAGESZ;
    for (int npages = NPAGE(fblen); npages-- ; ++frame)
        frames_incr(frame);
    frames_unlock();

    if ((err = paging_identity_map(bootinfo.framebuffer.framebuffer_addr,
                                   bootinfo.framebuffer.framebuffer_addr, GET_BOUNDARY_SIZE(0, fblen), VM_KRW | VM_PCD)))
    {
        if (lock)
            spinlock_free(lock);

        klog(KLOG_WARN, "linear framebuffer could not be mapped\n");
        return 0;
    }

    memset(&fix_info, 0, sizeof fix_info);
    memset(&var_info, 0, sizeof var_info);
    memset(&framebuffer, 0, sizeof framebuffer);

    fix_info = (fb_fixinfo_t){
        .addr = bootinfo.framebuffer.framebuffer_addr,
        .memsz = bootinfo.framebuffer.framebuffer_pitch * bootinfo.framebuffer.framebuffer_height,
        .id[0] = 'V',
        .id[1] = 'E',
        .id[2] = 'S',
        .id[3] = 'A',
        .id[4] = '_',
        .id[5] = 'V',
        .id[6] = 'B',
        .id[7] = 'E',
        .id[8] = '3',
        .line_length = bootinfo.framebuffer.framebuffer_pitch,
        .type = bootinfo.framebuffer.framebuffer_type,
    };

    var_info = (fb_varinfo_t){
        .colorspace = 1,
        .bpp = bootinfo.framebuffer.framebuffer_bpp,
        .pitch = bootinfo.framebuffer.framebuffer_pitch,
        .width = bootinfo.framebuffer.framebuffer_width,
        .height = bootinfo.framebuffer.framebuffer_height,

        .red = bootinfo.framebuffer.red,
        .blue = bootinfo.framebuffer.blue,
        .green = bootinfo.framebuffer.green,
        .transp = bootinfo.framebuffer.resv,
    };

    framebuffer[0].dev = &fbdev;
    framebuffer[0].lock = lock;
    framebuffer[0].priv = &bootinfo.framebuffer;
    framebuffer[0].module = 0;
    framebuffer[0].fixinfo = &fix_info;
    framebuffer[0].varinfo = &var_info;

    return 0;
}

int fbdev_mount(void)
{
    dev_attr_t attr = {
        .devid = *_DEVID(FS_CHRDEV, _DEV_T(DEV_FBDEV, 0)),
        .size = fix_info.memsz,
        .mask = 0666,
    };
    return devfs_mount("fbdev", attr);
}

int fbdev_open(struct devid *dd __unused, int mode __unused, ...)
{
    return 0;
}

int fbdev_close(struct devid *dd __unused)
{
    return 0;
}

int fbdev_ioctl(struct devid *dd, int req, void *argp)
{
    if (argp == NULL)
        return -EINVAL;

    if (dd->dev_minor >= NFBDEV)
        return -EINVAL;
    
    framebuffer_t *fb = &framebuffer[dd->dev_minor];

    switch (req)
    {
    case FBIOGET_FIX_INFO:
        spin_lock(fb->lock);
        memcpy(argp, (void *)fb->fixinfo, sizeof fix_info);
        spin_unlock(fb->lock);
        break;
    case FBIOGET_VAR_INFO:
        spin_lock(fb->lock);
        memcpy(argp, (void *)fb->varinfo, sizeof var_info);
        spin_unlock(fb->lock);
        break;
    default:
        return -EINVAL;
    }
    return 0;
}

size_t fbdev_read(struct devid *dd, off_t offset, void *buf, size_t sz)
{
    size_t size = 0;

    if (dd->dev_minor >= NFBDEV)
        return -EINVAL;

    if (buf == NULL)
        return 0;

    framebuffer_t *fb = &framebuffer[dd->dev_minor];

    spin_lock(fb->lock);
    size = MIN(sz, fb->fixinfo->memsz - offset);
    size = (size_t)(memcpy(buf, (void *)(fb->fixinfo->addr + offset), size) - buf);
    spin_unlock(fb->lock);
    return size;
}

size_t fbdev_write(struct devid *dd, off_t offset, void *buf, size_t sz)
{
    size_t size = 0;

    if (dd->dev_minor >= NFBDEV)
        return -EINVAL;

    if (buf == NULL)
        return 0;
    framebuffer_t *fb = &framebuffer[dd->dev_minor];
    spin_lock(fb->lock);
    size = MIN(sz, fb->fixinfo->memsz - offset);
    size = (size_t)(memcpy((void *)(fb->fixinfo->addr + offset), buf, size) - (fb->fixinfo->addr + offset));
    spin_unlock(fb->lock);
    return size;
}

int fbdev_mmap(file_t *file, vmr_t *region)
{
    int err = 0;
    off_t off = 0;
    size_t len = 0;
    int npages = 0;
    uintptr_t addr = 0;
    uintptr_t to_addr = 0;
    inode_t *inode = NULL;
    struct devid *dd = NULL;

    inode = file->f_inode;
    
    dd = _INODE_DEV(inode);

    if (dd->dev_minor >= NFBDEV)
        return -EINVAL;

    framebuffer_t *fb = &framebuffer[dd->dev_minor];

    if (__vmr_exec(region))
        return -EINVAL;

    if (!__vmr_read(region) && !__vmr_write(region))
        return -EINVAL;

    if (!__vmr_dontexpand(region))
        region->flags |= VM_DONTEXPAND;

    off = region->file_pos;
    addr = PGROUND(region->start);
    to_addr = PGROUND(fb->fixinfo->addr + off);
    len = MIN(MIN(region->filesz, __vmr_size(region)), fb->fixinfo->memsz - off);
    region->filesz = len;
    len = GET_BOUNDARY_SIZE(0, region->filesz);

    frames_lock();
    int frame = to_addr / PAGESZ;
    for (npages = NPAGE(len) + frame; frame < npages; )
        frames_incr(frame++);
    frames_unlock();

    if ((err = paging_identity_map(to_addr, addr, len, region->vflags)))
    {
        frames_lock();
        int frame = to_addr / PAGESZ;
        for (npages = NPAGE(len) + frame; frame < npages;)
            frames_decr(frame++);
        frames_unlock();
        return err;
    }

    return 0;
}

int fbdev_init(void)
{
    if (bootinfo.framebuffer.framebuffer_type == 1)
        return kdev_register(&fbdev, DEV_FBDEV, FS_CHRDEV);
    else
        return 0;
}

static struct dev fbdev = {
    .dev_name = "fbdev",
    .dev_probe = fbdev_probe,
    .dev_mount = fbdev_mount,
    .devid = _DEV_T(DEV_FBDEV, 0),
    .devops = {
        .open = fbdev_open,
        .read = fbdev_read,
        .write = fbdev_write,
        .ioctl = fbdev_ioctl,
        .close = fbdev_close,
    },

    .fops = {
        .close = posix_file_close,
        .ioctl = posix_file_ioctl,
        .lseek = posix_file_lseek,
        .open = posix_file_open,
        .perm = NULL,
        .read = posix_file_read,
        .sync = NULL,
        .stat = posix_file_ffstat,
        .write = posix_file_write,
        .mmap = fbdev_mmap,

        .can_read = (size_t(*)(struct file *, size_t))__always,
        .can_write = (size_t(*)(struct file *, size_t))__always,
        .eof = (size_t(*)(struct file *))__never,
    },
};

MODULE_INIT(fbdev, fbdev_init, NULL);