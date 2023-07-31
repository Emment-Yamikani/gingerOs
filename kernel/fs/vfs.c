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

static dentry_t *droot = NULL;
static queue_t *fs_list = QUEUE_NEW("filesystem list");

int vfs_init(void)
{
    int err = 0;
    inode_t *inode = NULL;
    printk("vfs initializing...\n");

    if ((err = dentry_alloc("/", &droot)))
        goto error;

    if ((err = tmpfs_init()))
        goto error;

    if ((err = devfs_init()))
        goto error;

    if ((err = ialloc(&inode)))
        goto error;

    inode->i_mask = 0777;
    inode->i_type = FS_DIR;

    if ((err = vfs_mount("/", "mnt", inode)))
        goto error;

    if ((err = vfs_lookupfs("tmpfs", &inode->ifs)))
        goto error;

    if ((err = ramfs_init()))
        goto error;

    if ((err = pipefs_init()))
        goto error;

    return 0;
error:
    printk("vfs_init(), error=%d\n", err);
    return err;
}

dentry_t *vfs_getroot_dentry(void)
{
    return droot;
}

int vfs_register(struct filesystem *fs)
{
    int err = 0;
    queue_node_t *node = NULL;

    assert(fs, "no filesystem pointer");
    assert(fs->fname, "filesystem descriptor has no name");
    assert(fs->fsuper, "filesystem descriptor has no superblock");

    queue_lock(fs_list);
    if ((node = enqueue(fs_list, (void *)fs)) == NULL)
    {
        err = -ENOMEM;
        queue_unlock(fs_list);
        goto error;
    }
    fs->flist_node = node;
    queue_unlock(fs_list);

    if (fs->load)
        if ((err = fs->load()))
            goto error;

    if (fs->fsmount)
        if ((err = fs->fsmount()))
            goto error;

    klog(KLOG_OK, "\'\e[0;012m%s\e[0m\' registered succefully\n", fs->fname);
    return 0;
error:
    klog(KLOG_FAIL, "failed to register filesystem \'\e[0;015m%s\e[0m\', error=%d\n", fs->fname, err);
    return err;
}

int vfs_lookupfs(const char *type, struct filesystem **ref)
{
    queue_node_t *next = NULL;
    struct filesystem *fs = NULL;

    if (ref == NULL || type == NULL)
        return -EINVAL;

    queue_lock(fs_list);
    forlinked(node, fs_list->head, next)
    {
        fs = node->data;
        next = node->next;

        if (!compare_strings(type, fs->fname))
        {
            *ref = fs;
            queue_unlock(fs_list);
            return 0;
        }
    }
    queue_unlock(fs_list);

    return -ENOENT;
}

static int vfs_path_syntax(char *path)
{
    if (!path)
        return -EINVAL;

    if (!*path)
        return -ENOTNAM;

    for (; *path; ++path)
    {
        if ((*path < (char)' ') || (*path == (char)'\\') || (*path == (char)0x7f) || (*path == (char)0x81) || (*path == (char)0x8D) || (*path == (char)0x9D) || (*path == (char)0xA0) || (*path == (char)0xAD))
            return -ENOTNAM;
    }
    return 0;
}

int vfs_canonicalize_path(char *fn, char *cwd, char ***ref)
{
    int err = 0;
    char *path = NULL;
    char **tokens = NULL;

    if (!fn || !cwd || !ref)
    {
        err = -EINVAL;
        goto error;
    }

    if ((err = vfs_parse_path(fn, cwd, &path)))
        goto error;

    if (!(tokens = canonicalize_path(path)))
    {
        err = -ENOMEM;
        goto error;
    }

    kfree(path);
    *ref = tokens;
    return 0;
error:
    if (tokens)
        tokens_free(tokens);
    if (path)
        kfree(path);
    printk("vfs_canonicalize_path(), called @ 0x%p, error=%d\n", return_address(0), err);
    return err;
}

int vfs_mount_root(inode_t *root)
{
    if (!root)
        return -EINVAL;
    if (droot->d_inode)
        iclose(droot->d_inode);
    droot->d_inode = root;
    return 0;
}

int vfs_parse_path(char *path, char *cwd, char **ref)
{
    int err = 0, j = 0, i = 0, tok_n = 0;
    size_t buflen = 0, cwdlen = 0, pathlen = 0;
    char *parsed_string = NULL, *tmp_cwd = NULL, **tokens = NULL, **tmp_path = NULL, *abs_path = NULL;

    if (!ref)
    {
        err = -EINVAL;
        goto error;
    }

    if ((err = vfs_path_syntax(path)))
        goto error;

    if (!cwd)
        cwd = "/";

    if (*path == '/')
        cwd = "/";

    if ((err = vfs_path_syntax(cwd)))
        goto error;

    if (*cwd != '/')
    {
        cwdlen = strlen(cwd);
        buflen = cwdlen + 2;
        if (!(tmp_cwd = __cast_to_type(tmp_cwd) kmalloc(buflen)))
        {
            err = -ENOMEM;
            goto error;
        }
        tmp_cwd[0] = '/';
        strncpy(tmp_cwd + 1, cwd, cwdlen);
        tmp_cwd[buflen - 1] = '\0';
        cwd = tmp_cwd;
    }

    cwdlen = strlen(cwd);
    pathlen = strlen(path);
    buflen = cwdlen + pathlen + 2;

    if (!(parsed_string = __cast_to_type(parsed_string) kmalloc(buflen)))
    {
        err = -ENOMEM;
        goto error;
    }

    strncpy(parsed_string, cwd, cwdlen);
    parsed_string[cwdlen] = '/';
    strncpy(parsed_string + cwdlen + 1, path, pathlen);
    parsed_string[buflen - 1] = 0;

    if (!(tokens = canonicalize_path(parsed_string)))
    {
        err = -ENOMEM;
        goto error;
    }

    foreach (token, tokens)
        tok_n++;

    if (!(tmp_path = __cast_to_type(tmp_path) kcalloc(tok_n + 1, sizeof(char *))))
    {
        err = -ENOMEM;
        goto error;
    }

    pathlen = 1;
    foreach (token, tokens)
    {
        if (!compare_strings(token, "."))
            continue;
        else if (!compare_strings(token, ".."))
        {
            if (i > 0)
            {
                token = tmp_path[--i];
                pathlen -= strlen(token) + 1;
                tmp_path[i] = NULL;
            }
        }
        else
        {
            tmp_path[i++] = token;
            pathlen += strlen(token) + 1;
        }
    }

    tmp_path[i] = NULL;

    switch (pathlen)
    {
    case 0:
        panic("vfs_parse_path(), caalled @ 0x%p, pathlen issue\n", return_address(0));
        break;
    case 1:
        if (!(abs_path = __cast_to_type(abs_path) kmalloc(2)))
        {
            err = -ENOMEM;
            goto error;
        }
        break;
    default:
        if (!(abs_path = __cast_to_type(abs_path) kmalloc(pathlen)))
        {
            err = -ENOMEM;
            goto error;
        }
        break;
    }

    foreach (token, tmp_path)
    {
        abs_path[j++] = '/';
        size_t tok_len = strlen(token);
        if (tok_len > FNAME_MAX)
        {
            err = -ENAMETOOLONG;
            goto error;
        }
        strncpy(abs_path + j, token, tok_len);
        j += tok_len;
    }
    abs_path[j] = '\0';

    if (!*abs_path)
        abs_path[j++] = '/';

    abs_path[j] = '\0';

    *ref = abs_path;

    kfree(tmp_path);
    tokens_free(tokens);
    kfree(parsed_string);
    if (tmp_cwd)
        kfree(tmp_cwd);

    return 0;

error:
    if (abs_path)
        kfree(abs_path);
    if (tmp_path)
        kfree(tmp_path);
    if (tokens)
        tokens_free(tokens);
    if (parsed_string)
        kfree(parsed_string);
    if (tmp_cwd)
        kfree(tmp_cwd);

    printk("vfs_parse_path(), called @ 0x%p, error=%d\n", return_address(0), err);
    return err;
}

int vfs_dentry_bind(dentry_t *parent, dentry_t *child)
{
    dentry_t *next = NULL;
    dentry_t *last = NULL;

    if (!parent || !child)
        return -EINVAL;

    if (!child->d_inode) // ensure child has an inode
        return -EINVAL;

    dlock(parent);

    forlinked(node, parent->d_children, next)
    {
        last = node;
        dlock(node);
        next = node->d_next;
        dunlock(node);
    }

    dlock(child);
    if (last)
    {
        dlock(last);
        last->d_next = child;
        child->d_prev = last;
        dunlock(last);
    }
    else
    {
        parent->d_children = child;
        child->d_prev = NULL;
    }
    child->d_next = NULL;
    child->d_parent = parent;

    dunlock(child);
    dunlock(parent);

    dentry_dup(parent);
    dentry_dup(child);

    return 0;
}

int vfs_dentry_unbind(dentry_t *parent, dentry_t *child)
{
    int err = 0;
    if (parent == NULL || child == NULL)
        return -EINVAL;

    dlock(parent);
    if ((err = dentry_contains(parent, child)) < 0)
    {
        dunlock(parent);
        return err;
    }

    dlock(child);

    if (child == parent->d_children)
        parent->d_children = child->d_next;

    if (child->d_prev)
    {
        dlock(child->d_prev);
        child->d_prev->d_next = child->d_next;
        dunlock(child->d_prev);
    }

    if (child->d_next)
    {
        dlock(child->d_next);
        child->d_next->d_prev = child->d_prev;
        dunlock(child->d_next);
    }

    child->d_parent = NULL;

    dunlock(child);
    dunlock(parent);

    return 0;
}

int vfs_path_dentry(const char *fn, dentry_t **ref)
{
    int err = 0;
    dentry_t *dentry = NULL;
    path_t *mountpoint = NULL;
    char **tokens = NULL, *abspath = NULL;

    if (!compare_strings("/", fn))
    {
        dentry = vfs_getroot_dentry();
        goto found;
    }

    if ((err = vfs_parse_path((char *)fn, "/", &abspath)))
        goto error;

    if ((tokens = canonicalize_path(abspath)) == NULL)
    {
        err = -ENOMEM;
        goto error;
    }

    if ((err = vfs_get_mountpoint(tokens, &mountpoint)))
        goto error;

    dentry = mountpoint->dentry;
found:
    kfree(abspath);
    kfree(mountpoint);
    tokens_free(tokens);
    *ref = dentry;
    return 0;
error:
    if (abspath)
        kfree(abspath);
    if (mountpoint)
        kfree(mountpoint);
    if (tokens)
        tokens_free(tokens);
    return err;
}

int vfs_get_mountpoint(char **abs_path, path_t **ref)
{
    path_t *path = NULL;
    char *last_entry = NULL;
    int err = 0, tok_i = 0, is_root = 1;
    dentry_t *children = NULL, *next = NULL, *dir = droot, *child = NULL;

    if (!abs_path || !ref)
        return -EINVAL;

    foreach (token, abs_path)
    {
        last_entry = token;
        is_root = 0;
    }

    foreach (token, abs_path)
    {
        child = NULL;
        dlock(dir);
        children = dir->d_children;
        dunlock(dir);
        tok_i++;
        forlinked(node, children, next)
        {
            dlock(node);
            if (!compare_strings(node->d_name, token))
            {
                child = node;
                dunlock(node);
                goto found;
            }
            else
                child = NULL;
            next = node->d_next;
            dunlock(node);
        }

        err = -ENOENT;
        break;

    found:
        if (last_entry == token)
            break;
        else
            dir = child;
    }

    if (!(path = __cast_to_type(path) kmalloc(sizeof *path)))
    {
        err = -ENOMEM;
        goto error;
    }

    *path = (path_t){0};
    path->dentry = child;
    if (is_root)
        path->dentry = droot;
    path->mnt.root = dir;
    path->tokens = abs_path + (tok_i - 1);

    *ref = path;
    if (err)
        goto error;
    return 0;
error:
    // printk("vfs_get_mountpoint(), called @ 0x%p, error=%d\n", return_address(0), err);
    return err;
}

int vfs_mount(const char *dir, const char *name, inode_t *ip)
{
    return vfs_mountat(name, dir, NULL, MS_BIND, NULL, ip, NULL);
}

int vfs_lookup(const char *fn, uio_t *uio, int oflags, mode_t mode __unused, inode_t **iref, dentry_t **dref)
{
    int err = 0;
    path_t *path = NULL;
    dentry_t *dentry = NULL, *dir = NULL;
    inode_t *iparent = NULL, *ichild = NULL;
    char **tokens = NULL, *cwd = NULL, *abs_path = NULL, *last_token = NULL;

    if (!fn || (!dref && !iref))
    {
        err = -EINVAL;
        goto error;
    }

    if (!uio)
        cwd = "/";
    else if (!uio->u_cwd)
        cwd = "/";
    else
        cwd = uio->u_cwd;

    if ((err = vfs_parse_path((char *)fn, cwd, &abs_path)))
        goto error;

    // printk("%s(%s): uio: 0x%p, oflags: %x, iref: 0x%p, dref: 0x%p\n", __func__, fn, uio, oflags, iref, dref);

    // printk("VFS ROOT: \e[017;0m%s\e[0m: %p\n", droot->d_name, droot->d_inode);

    if ((err = vfs_canonicalize_path((char *)abs_path, cwd, &tokens)))
        goto error;

    foreach(tok, tokens)
        last_token = tok;

    switch ((err = vfs_get_mountpoint(tokens, &path)))
    {
    case 0:
        dentry = path->dentry;
        ichild = dentry->d_inode;
        // printk("%s(): \'%s\' \e[0;12mfound\e[0m\n", __func__, dentry->d_name);
        // dentry_dump(dentry);
        if ((err = iperm(ichild, uio, oflags)))
            goto error;
        // printk("found THIS FILE\n");
        goto found;
    case -ENOENT:
        dir = path->mnt.root;
        iparent = dir->d_inode;
        // printk("\e[0;04mENOENT\e[0m: \e[0;11m");
        // foreach(tok, path->tokens) printk("/%s", tok);
        // printk("\e[0m\n");
        break;
    default:
        // printk("\e[0;04mlookup error\e[0m");
        goto error;
    }

    foreach (token, path->tokens)
    {
        dentry = NULL;
        ichild = NULL;
        if ((err = dentry_alloc(token, &dentry)))
            goto error;

        if ((err = ifind(iparent, token, &ichild))) {
            if ((oflags & O_CREAT) && (token == last_token))
                goto creat;
            goto error;
        }

        dentry->d_inode = ichild;

        if ((err = vfs_dentry_bind(dir, dentry)))
            goto error;

        if ((err = iperm(ichild, uio, oflags)))
            dir = dentry;
        iparent = ichild;
    }

found:
    kfree(path);
    tokens_free(tokens);
    kfree(abs_path);

    if (iref)
    {
        iincrement(ichild);
        *iref = ichild;
    }

    if (dref)
    {
        dentry_dup(dentry);
        *dref = dentry;
    }
    return 0;

creat:
    if ((err = icreate(iparent, dentry, mode)))
        goto error;

    ichild = dentry->d_inode;

    if ((err = vfs_dentry_bind(dir, dentry)))
        goto error;

    goto found;

error:
    if (dentry)
        dentry_close(dentry);
    if (path)
        kfree(path);
    if (tokens)
        tokens_free(tokens);
    if (abs_path)
        kfree(abs_path);
    printk("%s(%s), called @ 0x%p, error=%d\n", __func__, fn, return_address(0), err);
    return err;
}

int vfs_open(const char *fn, uio_t *uio, int oflags, mode_t mode, inode_t **iref)
{
    return vfs_lookup(fn, uio, oflags, mode, iref, NULL);
}