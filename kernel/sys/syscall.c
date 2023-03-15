#include <sys/_syscall.h>
#include <printk.h>
#include <sys/system.h>
#include <bits/errno.h>
#include <sys/thread.h>
#include <arch/i386/cpu.h>
#include <sys/proc.h>
#include <fs/sysfile.h>
#include <sys/sysprot.h>
#include <sys/sleep.h>
#include <mm/vmm.h>
#include <sys/sysproc.h>
#include <dev/pty.h>
#include <sys/_wait.h>

static uintptr_t (*syscall[])(void) = {
    /*File Management*/

    [SYS_KPUTC](void *) sys_kputc,
    [SYS_OPEN](void *) sys_open,
    [SYS_READ](void *) sys_read,
    [SYS_WRITE](void *) sys_write,
    [SYS_LSEEK](void *) sys_lseek,
    [SYS_CLOSE](void *) sys_close,
    [SYS_GETCWD](void *) sys_getcwd,
    [SYS_CHDIR](void *) sys_chdir,
    [SYS_PIPE](void *) sys_pipe,
    [SYS_DUP](void *) sys_dup,
    [SYS_DUP2](void *) sys_dup2,
    [SYS_FSTAT](void *) sys_fstat,
    [SYS_STAT](void *) sys_stat,
    [SYS_IOCTL](void *) sys_ioctl,
    [SYS_CREAT](void *) sys_creat,
    [SYS_READDIR](void *)sys_readdir,

    /*Process management*/

    [SYS_FORK](void *) sys_fork,
    [SYS_EXIT](void *) sys_exit,
    [SYS_WAIT](void *) sys_wait,
    [SYS_WAITPID](void *)sys_waitpid,
    [SYS_SLEEP](void *) sys_sleep,
    [SYS_EXECV](void *) sys_execv,
    [SYS_EXECVE](void *) sys_execve,
    [SYS_GETPID](void *) sys_getpid,

    /*Memory Management*/

    [SYS_SBRK](void *) sys_sbrk,
    [SYS_GETPAGESIZE](void *) sys_getpagesize,
    [SYS_MMAP] (void *)sys_mmap,
    [SYS_MUNMAP] (void *)sys_munmap,

    /*Thread management*/

    [SYS_THREAD_YIELD](void *) sys_thread_yield,
    [SYS_THREAD_CREATE](void *) sys_thread_create,
    [SYS_THREAD_SELF](void *) sys_thread_self,
    [SYS_THREAD_JOIN](void *) sys_thread_join,
    [SYS_THREAD_EXIT](void *) sys_thread_exit,
    [SYS_THREAD_CANCEL](void *)sys_thread_cancel,

    /*Protection*/

    [SYS_GETUID](void *) sys_getuid,
    [SYS_SETUID](void *) sys_setuid,
    [SYS_GETGID](void *) sys_getgid,
    [SYS_SETGID](void *) sys_setgid,
    [SYS_CHOWN] (void *) sys_chown,
    [SYS_FCHOWN] (void *) sys_fchown,
    
    /*Process relationships*/

    [SYS_GETPPID](void *) sys_getppid,
    [SYS_SETPGID](void *) sys_setpgid,
    [SYS_SETPGRP](void *) sys_setpgrp,
    [SYS_GETPGID](void *) sys_getpgid,
    [SYS_SETSID] (void *) sys_setsid,
    [SYS_GETSID] (void *) sys_getsid,
    [SYS_GETPGRP](void *) sys_getpgrp,

    /*Signals*/

    [SYS_PAUSE] (void *)sys_pause,
    [SYS_SIGNAL](void *)sys_signal,
    [SYS_KILL] (void *)sys_kill,

    /*Pseudoterminals*/

    [SYS_ISATTY] (void *)sys_isatty,
    [SYS_GRANTPT](void *)sys_grantpt,
    [SYS_OPENPT] (void *)sys_openpt,
    [SYS_UNLOCKPT](void *)sys_unlockpt,
    [SYS_PTSNAME_R](void *)sys_ptsname_r,
};

static int sys_syscall_ni(trapframe_t *tf)
{
    klog(KLOG_WARN, "syscall(%d) not implemented\n", tf->eax);
    return -ENOSYS;
}

static int sys_syscall_err(trapframe_t *tf)
{
    klog(KLOG_WARN, "invalid syscall(%d)\n", tf->eax);
    return -ENOSYS;
}

void syscall_stub(trapframe_t *tf)
{
    if ((tf->eax > NELEM(syscall)))
        tf->eax = sys_syscall_ni(tf);
    else if ((int)tf->eax < 0 || !syscall[tf->eax])
        tf->eax = sys_syscall_err(tf);
    else
        tf->eax = syscall[tf->eax]();
}

int chk_addr(uintptr_t addr)
{
    current_assert();
    if (addr == 0)
        return 0;
    mmap_lock(current->mmap);
    if (mmap_find(current->mmap, addr) == NULL)
    {
        mmap_unlock(current->mmap);
        return -EADDRNOTAVAIL;
    }
    mmap_unlock(current->mmap);
    return 0;
}

int fetchint(uintptr_t addr, int *p)
{
    int err = 0;
    if ((err = chk_addr(addr)))
        return err;
    *p = *(int *)addr;
    return 0;
}

int fetchstr(uintptr_t addr, char **pp)
{
    int err = 0, len = 0;
    char *str = (char *)addr, *end = (char *)USER_STACK;

    if ((err = chk_addr(addr)))
        return err;

    for (; (str < end) && *str; ++len, ++str)
    {
        if ((err = chk_addr(addr)))
            return err;
    }

    *pp = (char *)addr;
    return len;
}

int argint(int n, int *p)
{
    current_assert();
    trapframe_t *tf = current->t_tarch->tf;
    return fetchint(tf->esp + 4 + n * 4, p);
}

int argstr(int n, char **str)
{
    int err = 0;
    int addr = 0;
    if ((err = argint(n, &addr)))
        return err;
    return fetchstr(addr, str);
}

int argptr(int n, void **pp, int size)
{
    int err = 0;
    int addr = 0;
    current_assert();
    if ((err = argint(n, &addr)))
        return err;
    if ((err = chk_addr(addr + size)))
        return err;
    *pp = (void *)addr;
    return 0;
}

int sys_kputc(void)
{
    int c;
    assert(!argint(0, &c), "EADDRNOTAVAIL");
    return printk("%c", c);
}

/*File Management*/

int sys_open(void)
{
    int oflags = 0;
    char *path = NULL;
    assert(argstr(0, &path) > 0, "err fetching argstr");
    assert(!argint(1, &oflags), "err fetching argint");
    return open(path, oflags);
}

int sys_read(void)
{
    int fd = 0;
    size_t sz = 0;
    void *buf = NULL;
    assert(!argint(0, &fd), "err fetching fd");
    assert(!argptr(1, &buf, sizeof(void *)), "err fetching ptr");
    assert(!argint(2, (int *)&sz), "err fetching nbytes");
    return read(fd, buf, sz);
}

int sys_write(void)
{
    int fd = 0;
    size_t sz = 0;
    void *buf = NULL;
    assert(!argint(0, &fd), "err fetching fd");
    assert(!argptr(1, &buf, sizeof(void *)), "err fetching ptr");
    assert(!argint(2, (int *)&sz), "err fetching nbytes");
    return write(fd, buf, sz);
}

off_t sys_lseek(void)
{
    int fd = 0;
    int whence = 0;
    off_t offset = 0;
    assert(!argint(0, &fd), "err fetching fd");
    assert(!argint(1, (int *)&offset), "err fetching offset");
    assert(!argint(2, &whence), "err fetching whence");
    return lseek(fd, offset, whence);
}

int sys_close(void)
{
    int fd = 0;
    assert(!argint(0, &fd), "err fetching fd");
    return close(fd);
}

int sys_readdir(void){
    int fd = 0;
    struct dirent *dirent = NULL;
    assert(!argint(0, (int *)&fd), "err fetching fd");
    assert(!argptr(1, (void **)&dirent, sizeof(char *)), "err fetching dirent");
    return readdir(fd, dirent);
}

char *sys_getcwd(void)
{
    size_t sz = 0;
    char *buf = NULL;
    assert(!argptr(0, (void **)&buf, sizeof(char *)), "err fetching buf");
    assert(!argint(1, (int *)&sz), "err fetching nbytes");
    return getcwd(buf, sz);
}

int sys_chdir(void)
{
    char *dir = NULL;
    assert(argstr(0, &dir) > 0, "err fetching dir");
    return chdir(dir);
}

int sys_pipe(void)
{
    int *p = NULL;
    assert(!argptr(0, (void **)&p, sizeof(int *)), "err fetching ptr");
    return pipe(p);
}

int sys_dup(void)
{
    int fd = 0;
    assert(!argint(0, &fd), "err fetching fd");
    return dup(fd);
}

int sys_dup2(void)
{
    int fd = 0;
    int fd1 = 0;
    assert(!argint(0, &fd), "err fetching fd");
    assert(!argint(1, &fd1), "err fetching fd");
    return dup2(fd, fd1);
}

int sys_fstat(void)
{
    int fd = 0;
    struct stat *buf = NULL;
    assert(!argint(0, &fd), "err fetching fd");
    assert(!argptr(1, (void **)&buf, sizeof(struct stat *)), "err fetching \'struct stat *\'");
    return fstat(fd, buf);
}

int sys_stat(void)
{
    char *path = NULL;
    struct stat *buf = NULL;
    assert(argstr(0, &path) > 0, "err fetching path");
    assert(!argptr(1, (void **)&buf, sizeof(struct stat *)), "err fetching \'struct stat *\'");
    return stat(path, buf);
}

int sys_ioctl(void)
{
    int fd = 0;
    long request = 0;
    void *argp = NULL;

    assert(!argint(0, &fd), "err fetching fd");
    assert(!argint(1, (int *)&request), "err fetching request");
    assert(!argptr(2, &argp, sizeof(void *)), "err fetching argp");
    return ioctl(fd, request, argp);
}

int sys_creat(void)
{
    int mode = 0;
    char *path = NULL;
    assert(argstr(0, &path) > 0, "err fetching path");
    assert(!argint(1, &mode), "err fetching mode");
    return -ENOSYS; // creat(path, mode);
}

/*process management*/

pid_t sys_getpid(void)
{
    return getpid();
}

long sys_sleep(void)
{
    long ms = 0;
    assert(!argint(0, (int *)&ms), "err fetching ms");
    return sleep(ms);
}

void sys_exit(void)
{
    int status = 0;
    assert(!argint(0, &status), "err fetching exit code");
    exit(status);
    thread_exit(status);
}

pid_t sys_fork(void)
{
    return fork();
}

pid_t sys_wait(void)
{
    int *staloc = NULL;
    assert(!argptr(0, (void **)&staloc, sizeof(int *)), "err fetching staloc");
    return wait(staloc);
}

pid_t sys_waitpid(void)
{
    pid_t pid = 0;
    int *stat_loc = NULL;
    int options = 0;
    argint(0, &pid);
    argptr(1, (void **)&stat_loc, sizeof(int *));
    argint(2, &options);
    return waitpid(pid, stat_loc, options);
}

int sys_execve(void)
{
    char *path = NULL;
    char **argp = NULL;
    char **envp = NULL;
    assert(!argstr(0, &path), "err fetching execv");
    assert(!argptr(1, (void **)&argp, sizeof(char **)), "err fetching argp");
    assert(!argptr(1, (void **)&envp, sizeof(char **)), "err fetching envp");
    return execve(path, (const char **)argp, (const char **)envp);
}

int sys_execv(void)
{
    char *path = NULL;
    char **argv = NULL;
    assert(argstr(0, &path) > 0, "err fetching execv");
    assert(!argptr(1, (void **)&argv, sizeof(char **)), "err fetching argv");
    return execv(path, (const char **)argv);
}


/*Memery management*/
#include <sys/mman/mman.h>

void *sys_sbrk(void)
{
    ptrdiff_t incr = 0;
    assert(!argint(0, (int *)&incr), "err fetching incr");
    return sbrk(incr);
}

int sys_getpagesize(void)
{
    return getpagesize();
}

void *sys_mmap(void)
{
    uintptr_t addr = 0;
    size_t length = 0;
    int prot = 0;
    int flags = 0;
    int fd = 0;
    off_t offset = 0;
    assert(!argint(0, (int *)&addr), "err fetching addr");
    assert(!argint(1, (int *)&length), "err fetching length");
    assert(!argint(2, (int *)&prot), "err fetching prot");
    assert(!argint(3, (int *)&flags), "err fetching flags");
    assert(!argint(4, (int *)&fd), "err fetching fd");
    assert(!argint(5, (int *)&offset), "err fetching offset");
    return mmap((void *)addr, length, prot, flags, fd, offset);
}

int sys_munmap(void)
{
    uintptr_t addr = 0;
    size_t length = 0;
    assert(!argint(0, (int *)&addr), "err fetching addr");
    assert(!argint(1, (int *)&length), "err fetching length");
    return -ENOSYS;
    //return munmap((void *)addr, length);
}

/*Thread Management*/

void sys_thread_yield(void)
{
    thread_yield();
}

int sys_thread_create(void)
{
    uintptr_t arg = 0;
    tid_t *tid = NULL;
    void *(*entry)(void *) = NULL;
    assert(!argptr(0, (void **)&tid, sizeof(tid_t *)), "err fetching tid");
    assert(!argptr(1, (void **)&entry, sizeof(void *(*)(void *))), "err fetching entry point");
    assert(!argint(2, (int *)&arg), "err fetching arg");
    return thread_create(tid, entry, (void *)arg);
}

tid_t sys_thread_self(void)
{
    return thread_self();
}

int sys_thread_join(void)
{
    tid_t tid = 0;
    void **retval = NULL;
    assert(!argint(0, &tid), "err fetching tid");
    assert(!argptr(1, (void **)&retval, sizeof(void **)), "err fetching retval-ptr");
    return thread_join(tid, retval);
}

void sys_thread_exit(void)
{
    uintptr_t retval = 0;
    assert(!argint(0, (int *)&retval), "err fetching retval-ptr");
    thread_exit((uintptr_t)retval);
}

int sys_thread_cancel(void)
{
    tid_t thread = 0;
    argint(0, &thread);

    return thread_cancel(thread);
}

/*Protection*/

uid_t sys_getuid(void)
{
    return getuid();
}

gid_t sys_getgid(void)
{
    return getgid();
}

int sys_setgid(void)
{
    gid_t gid = 0;
    assert(!argint(0, &gid), "err fetching gid");
    return setgid(gid);
}

int sys_setuid(void)
{
    uid_t uid = 0;
    assert(!argint(0, &uid), "err fetching uid");
    return setuid(uid);
}

int sys_chown(void)
{
    char *pathname = NULL;
    uid_t uid = 0;
    gid_t gid = 0;
    assert(argstr(0, &pathname) > 0, "err fetching pathname");
    assert(!argint(1, &uid), "err fetching uid");
    assert(!argint(2, &gid), "err fetching gid");
    return chown(pathname, uid, gid);
}

int sys_fchown(void)
{
    int fd = 0;
    uid_t uid = 0;
    gid_t gid = 0;
    assert(!argint(0, &fd), "err fetching fd");
    assert(!argint(1, &uid), "err fetching uid");
    assert(!argint(2, &gid), "err fetching gid");
    return fchown(fd, uid, gid);
}

/*Process relationships*/

pid_t sys_getppid(void)
{
    return getppid();
}

pid_t sys_setpgrp(void)
{
    return setpgrp();
}

pid_t sys_getpgid(void)
{
    pid_t pid = 0;
    assert(!argint(0, &pid), "err fetching pid");
    return getpgid(pid);
}

pid_t sys_setpgid(void)
{
    pid_t pid = 0, pgid = 0;
    assert(!argint(0, &pid), "err fetching pid");
    assert(!argint(1, &pgid), "err fetching pgid");
    return setpgid(pid, pgid);
}

pid_t sys_setsid(void)
{
    return setsid();
}

pid_t sys_getsid(void)
{
    pid_t pid = 0;
    assert(!argint(0, &pid), "err fetching pid");
    return getsid(pid);
}

pid_t sys_getpgrp(void)
{
    return getpgrp();
}

/*Signal*/

#include <sys/_signal.h>

void (*sys_signal(void))(int)
{
    pid_t pid = 0;
    void (*handler)(int);
    assert(!argint(0, &pid), "err fetching pid");
    assert(!argptr(1, (void **)&handler, sizeof handler), "err fetching handler");
    return signal(pid, handler);
}

int sys_pause(void)
{
    return pause();
}

int sys_kill(void)
{
    pid_t pid = 0;
    int sig = 0;
    assert(!argint(0, &pid), "err fetching pid");
    assert(!argint(1, &sig), "err fetching sig");
    return kill(pid, sig);
}

int sys_isatty(void)
{
    int fd = 0;
    assert(!argint(0, &fd), "err fetching fd");
    return isatty(fd);
}

int sys_ptsname_r(void)
{
    int fd = 0;
    char *buf = NULL;
    size_t size = 0;
    assert(!argint(0, &fd), "err fetching fd");
    assert(!argptr(1, (void **)&buf, sizeof buf), "err fetching buf");
    assert(!argint(2, (int *)&size), "err fetching size");
    return ptsname_r(fd, buf, size);
}

int sys_grantpt(void)
{
    int fd = 0;
    assert(!argint(0, &fd), "err fetching fd");
    return grantpt(fd);
}

int sys_unlockpt(void)
{
    int fd = 0;
    assert(!argint(0, &fd), "err fetching fd");
    return unlockpt(fd);
}

int sys_openpt(void)
{
    int status = 0;
    assert(!argint(0, &status), "err fetching status");
    return openpt(status);
}