/*
 *          VFS => Generic file read function
 *
 *
 *  This file is part of Aquila OS and is released under
 *  the terms of GNU GPLv3 - See LICENSE.
 *
 */

//#include <core/system.h>

#include <fs/fs.h>
#include <sys/fcntl.h>
#include <bits/errno.h>
#include <locks/cond.h>
#include <sys/thread.h>
#include <sys/proc.h>
#include <arch/i386/cpu.h>
//#include <bits/dirent.h>

/**
 * posix_file_read
 *
 * Reads up to `size' bytes from a file to `buf'.
 * Conforming to `IEEE Std 1003.1, 2013 Edition'
 * 
 * @file    File Descriptor for the function to operate on.
 * @buf     Buffer to write to.
 * @size    Number of bytes to read.
 * @returns read bytes on success, or error-code on failure.
 */

size_t posix_file_read(struct file *file, void *buf, size_t size)
{
    if (file->f_flags & O_WRONLY) /* File is not opened for reading */
        return -EBADFD;

    if (!size)
        return 0;
    
    int retval;
    for (;;) {
        if ((retval = iread(file->f_inode, file->f_pos, buf, size)) > 0) {
            /* Update file offset */
            file->f_pos += retval;
            //printk("[%d:%d] posix signal writers\n", proc->pid, current->t_tid);
            /* Wake up all sleeping writers if a `write_queue' is attached */
            if (file->f_inode->i_writers)
                cond_broadcast(file->f_inode->i_writers);
            /* Return read bytes count */
            return retval;
        } else if (retval < 0) {    /* Error */
            return retval;
        } else if (feof(file)) {
            printk("[%d:%d] posix EOF\n", proc->pid, current->t_tid);
            /* Reached end-of-file */
            return 0;
        } else if (file->f_flags & O_NONBLOCK) {
            /* Can not satisfy read operation, would block */
            return -EAGAIN;
        } else {
            /* Block until some data is available */
            /* Sleep on the file readers queue */
            if (file->f_inode->i_readers){
                printk("[%d:%d] posix waiting for writers\n", proc->pid, current->t_tid);
                if ((retval = cond_wait(file->f_inode->i_readers)))
                    return retval;
            }
        }
    }
    return retval;
}
