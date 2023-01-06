/*
 *          VFS => Generic file readdir function
 *
 *
 *  This file is part of Aquila OS and is released under
 *  the terms of GNU GPLv3 - See LICENSE.
 *
 *  Copyright (C) 2016-2017 Mohamed Anwar <mohamed_anwar@opmbx.org>
 *
 */

#include <sys/system.h>

#include <fs/fs.h>

#include <bits/fcntl.h>
#include <bits/errno.h>
#include <bits/dirent.h>


/**
 * posix_file_readdir
 *
 * Conforming to `IEEE Std 1003.1, 2013 Edition'
 * 
 * @file    File Descriptor for the function to operate on.
 * @dirent  Buffer to write to.
 */

long posix_file_readdir(struct file *file, struct dirent *dirent)
{
    if (file->f_flags & O_WRONLY) /* File is not opened for reading */
        return -EBADFD;
    
    int retval = vfs_ireaddir(file->f_dentry->d_inode, file->f_pos, dirent);

    /* Update file offset */
    file->f_pos += retval;
        
    /* Return read bytes count */
    return retval;
}
