#include <stdlib.h>
#include <bits/dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <bits/errno.h>
#include <ginger.h>

int sys_readdir(int fd, struct dirent *);

DIR *fdopendir(int fd)
{
    int err = 0;
    struct stat buf;
    DIR *dirp = NULL;    
    
    memset(&buf, 0, sizeof buf);
    if((err = fstat(fd, &buf)) < 0)
        return NULL;

    if ((buf.st_mode & _IFDIR) == 0)
        return NULL;

    if ((dirp = malloc(sizeof *dirp)) == NULL)
        return NULL;
    
    dirp->fd = fd;
    dirp->next = -1;

    return dirp;
}

DIR *opendir(const char *fn)
{
    int fd = 0;
    DIR *dirp = NULL;
    if ((fd = open(fn, O_RDWR)) < 0)
        return NULL;
    return (dirp = fdopendir(fd)) ? dirp : NULL; 
}

struct dirent *readdir(DIR *dirp)
{
    int err = 0;
    struct dirent *dirent = NULL;

    if (dirp == NULL)
        return NULL;

    if ((dirent = malloc(sizeof *dirent)) == NULL)
        return NULL;

    if ((err = sys_readdir(dirp->fd, dirent))){
        free(dirent);
        return NULL;
    }
    return dirent;
}

int closedir(DIR *dirp)
{
    int err = 0;
    if ((err = close(dirp->fd)))
        return err;
    free(dirp);
    return err;
}
