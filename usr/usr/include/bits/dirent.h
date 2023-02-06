#ifndef _BITS_DIRENT_H
#define _BITS_DIRENT_H

#include <stdint.h>
#include <stddef.h>
#include <types.h>

#define MAX_NAME_LEN 256


struct dirent
{
    ino_t d_ino;             /* Inode number */
    off_t d_off;             /* Not an offset; see below */
    unsigned short d_reclen; /* Length of this record */
    unsigned char d_type;    /* Type of file; not supported
                                by all filesystem types */
    char d_name[MAX_NAME_LEN];        /* Null-terminated filename */
};

typedef struct 
{
    int fd; // open file descriptoy
    int next;
} DIR;

DIR *fdopendir(int fd);
DIR *opendir(const char *fn);
struct dirent *readdir(DIR *dir);
int closedir(DIR *dir);


#endif
