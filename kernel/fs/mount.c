#include <bits/errno.h>
#include <fs/fs.h>
#include <fs/dentry.h>
#include <fs/devfs.h>
#include <fs/ramfs.h>
#include <lib/string.h>
#include <lime/string.h>
#include <mm/kalloc.h>
#include <printk.h>
#include <fs/pipefs.h>
#include <fs/tmpfs.h>

int vfs_mountat(const char *__src, const char *__target,
                const char *__type, uint32_t __mount_flags,
                const void *__data, inode_t *__inode, uio_t *__uio)
{
    int err = 0;
    struct filesystem *fs = NULL;
    char **target_tokens __unused = NULL, *src_tokens __unused = NULL;
    dentry_t *parent_dentry = NULL, *child_dentry = NULL;
    path_t *target_mountpoint __unused = NULL, *src_mountpoint __unused = NULL;
    char *cwd = NULL, *abs_path_src __unused = NULL, *abs_path_target = NULL;

    if (__uio && (__uio->u_uid && __uio->u_gid))
        return -EPERM;

    if (!__uio)
        cwd = "/";
    else if (!__uio->u_cwd)
        cwd = "/";
    else
        cwd = __uio->u_cwd;

    if ((err = vfs_parse_path((char *)__target, cwd, &abs_path_target)))
        goto error;

    if (__mount_flags & MS_REMOUNT)
    {
    }

    if (__mount_flags & MS_BIND)
    {
        //! TODO: lock dentry before passing it to us
        if ((err = vfs_path_dentry(abs_path_target, &parent_dentry)))
            goto error;

        // want to bind inode
        if (__inode)
        {
            if ((err = dentry_alloc((char *)__src, &child_dentry)))
                goto error;

            if ((err = iincrement(__inode)))
                goto error;

            // TODO: handle error from imount
            imount(parent_dentry->d_inode, __src, __inode);

            child_dentry->d_inode = __inode;
            __inode->i_dentry = child_dentry;

            if ((err = vfs_dentry_bind(parent_dentry, child_dentry)))
                goto error;

            goto done;
        }

        goto error; // TODO: handle filesystem spec bind mount
    }

    if (__mount_flags & MS_MOVE)
    {
        if ((err = vfs_parse_path((char *)__src, cwd, &abs_path_src)))
            goto error;

        //! TODO: lock dentry before passing it to us
        if ((err = vfs_path_dentry(abs_path_src, &child_dentry)))
            goto error;

        if ((err = vfs_dentry_unbind(child_dentry->d_parent, child_dentry)))
            goto error;

        if ((err = vfs_path_dentry(abs_path_target, &parent_dentry)))
            goto error;

        if ((err = vfs_dentry_bind(parent_dentry, child_dentry)))
            goto error;

        goto done;
    }

    if (MS_NONE(__mount_flags))
    {
        if ((err = vfs_lookupfs(__type, &fs)))
            goto error;
        return fs->mount(abs_path_src, abs_path_target, __mount_flags, __data);
    }

    if (__type)
    { // performe a filesystem speific mount()
        if ((err = vfs_lookupfs(__type, &fs)))
            goto error;
    }

done:
    if (abs_path_src)
        kfree(abs_path_src);

    if (abs_path_target)
        kfree(abs_path_target);

    return 0;
error:
    if (child_dentry)
        dentry_close(child_dentry);
    if (abs_path_target)
        kfree(abs_path_target);
    return err;
}