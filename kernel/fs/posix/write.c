/*
 *			VFS => Generic file write function
 *
 *
 *	This file is part of Aquila OS and is released under
 *	the terms of GNU GPLv3 - See LICENSE.
 *
 *	Copyright (C) 2016 Mohamed Anwar <mohamed_anwar@opmbx.org>
 */

//#include <core/system.h>

#include <fs/fs.h>
#include <locks/cond.h>
#include <printk.h>
#include <sys/fcntl.h>
#include <lib/errno.h>
//#include <bits/dirent.h>

/**
 * posix_file_write
 *
 * Writes up to `size' bytes from `buf' into a file.
 *
 * @file 	File Descriptor for the function to operate on.
 * @buf  	Buffer to read from.
 * @size 	Number of bytes to write.
 * @returns written bytes on success, or error-code on failure.
 */

long posix_file_write(struct file *file, void *buf, long size)
{
	if (!(file->f_flags & (O_WRONLY | O_RDWR)))	/* File is not opened for writing */
		return -EBADFD;
	
	if (file->f_flags & O_NONBLOCK) {	/* Non-blocking I/O */
		if (vfs_fcan_write(file, size)) {
			/* write up to `size' from `buf' into file */
			long retval = iwrite(file->f_inode, file->f_pos, buf, size);

			/* Update file offset */
			file->f_pos += retval;
			
			/* Wake up all sleeping readers if a `read_queue' is attached */
			if (file->f_inode->i_rwait)
				cond_signal(file->f_inode->i_rwait);

			/* Return written bytes count */
			return retval;
		} else {
			/* Can not satisfy write operation, would block */
			return -EAGAIN;
		}
	} else {	/* Blocking I/O */
		long retval = size;

		while (size) {
			size -= iwrite(file->f_inode, file->f_pos, buf, size);

			/* No bytes left to be written, or reached END-OF-FILE */
			if (!size || feof(file))	/* Done writting */
				break;

			/* Sleep on the file writers queue */
			cond_wait(file->f_inode->i_wwait);
		}
		
		/* Store written bytes count */
		retval -= size;

		/* Update file offset */
		file->f_pos += retval;

		/* Wake up all sleeping readers if a `read_queue' is attached */
		if (file->f_inode->i_rwait)
			cond_signal(file->f_inode->i_rwait);

		return retval;
	}
}
