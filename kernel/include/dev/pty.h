#ifndef DEV_PTY_H
#define DEV_PTY_H 1

#include <fs/fs.h>
#include <ds/ringbuf.h>
#include <locks/spinlock.h>
#include <dev/tty.h>
#include <lime/assert.h>
#include <sys/system.h>


typedef struct pty
{
    int id;

    inode_t *master;
    inode_t *slave;

    //ringbuf_t *in;
    //ringbuf_t *out;

    struct {
        inode_t *read;  // master's from in-end
        inode_t *write; // to slave's in-end
    } master_io; // with reference to the slave
    
    struct {
        inode_t *read; // slave's from in-end
        inode_t *write; // to master's in-end
    } slave_io; // with reference to the slave

    spinlock_t *lock;

    struct tty *tty;

    int slave_unlocked;
}pty_t, *PTY;

extern struct pty *ptytable[256];
extern spinlock_t *ptylk;

#define pty_assert(pty) assert(pty, "no PTY")
#define pty_assert_lock(pty) {pty_assert(pty); spin_assert_lock(pty->lock);}
#define pty_lock(pty) {pty_assert(pty); spin_lock(pty->lock);}
#define pty_unlock(pty) {pty_assert(pty); spin_unlock(pty->lock);}


int pty_new(proc_t *process, inode_t **master, char **ref);

/*Pseudo terminals*/

int isatty(int fd);
int grantpt(int fd);
int openpt(int flags);
int unlockpt(int fd);
int ptsname_r(int fd, char *buf, size_t buflen);

#endif // DEV_PTY_H