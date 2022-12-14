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

static uintptr_t (*syscall[])(void) =
    {
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

        /*Process management*/

        [SYS_GETPID](void *) sys_getpid,
        [SYS_SLEEP](void *) sys_sleep,
        [SYS_EXIT](void *) sys_exit,
        [SYS_FORK](void *) sys_fork,
        [SYS_WAIT](void *) sys_wait,
        [SYS_EXECVE](void *) sys_execve,
        [SYS_EXECV](void *) sys_execv,
        [SYS_SBRK](void *) sys_sbrk,

        /*Process relationships*/

        [SYS_GETPPID](void *) sys_getppid,

        /*Memory Management*/

        [SYS_GETPAGESIZE](void *) sys_getpagesize,

        /*Thread management*/

        [SYS_THREAD_YIELD](void *) sys_thread_yield,
        [SYS_THREAD_CREATE](void *) sys_thread_create,
        [SYS_THREAD_SELF](void *) sys_thread_self,
        [SYS_THREAD_JOIN](void *) sys_thread_join,
        [SYS_THREAD_EXIT](void *) sys_thread_exit,

        /*Protection*/

        [SYS_GETUID](void *) sys_getuid,
        [SYS_SETUID](void *) sys_setuid,
        [SYS_GETGID](void *) sys_getgid,
        [SYS_SETGID](void *) sys_setgid,
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
    shm_assert_lock(current->mmap);
    if (shm_lookup(current->mmap, addr) == NULL)
        return -EADDRNOTAVAIL;
    return 0;
}

int fetchint(uintptr_t addr, int *p)
{
    int err = 0;
    shm_lock(current->mmap);
    if ((err = chk_addr(addr)))
    {
        shm_unlock(current->mmap);
        return err;
    }
    *p = *(int *)addr;
    shm_unlock(current->mmap);
    return 0;
}

int fetchstr(uintptr_t addr, char **pp)
{
    int err = 0, len = 0;
    char *str = (char *)addr, *end = (char *)USER_STACK;

    shm_lock(current->mmap);
    if ((err = chk_addr(addr)))
    {
        shm_unlock(current->mmap);
        return err;
    }

    for (; (str < end) && *str; ++len, ++str)
    {
        if ((err = chk_addr(addr)))
        {
            shm_unlock(current->mmap);
            return err;
        }
    }

    shm_unlock(current->mmap);
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
    shm_lock(current->mmap);
    if ((err = chk_addr(addr + size)))
    {
        shm_unlock(current->mmap);
        return err;
    }
    shm_unlock(current->mmap);
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
    int ret = wait(staloc);
    //printk("wait pid: %d, eip: %p\n", ret, ((trapframe_t *)current->t_tarch->tf)->eip);
    return ret;
}

int sys_execve(void)
{
    char *path = NULL;
    char **argp = NULL;
    char **envp = NULL;
    assert(!argstr(0, &path), "err fetching execv");
    assert(!argptr(1, (void **)&argp, sizeof(char **)), "err fetching argp");
    assert(!argptr(1, (void **)&envp, sizeof(char **)), "err fetching envp");
    return execve(path, argp, envp);
}

int sys_execv(void)
{
    char *path = NULL;
    char **argv = NULL;
    assert(argstr(0, &path) > 0, "err fetching execv");
    assert(!argptr(1, (void **)&argv, sizeof(char **)), "err fetching argv");
    return execv(path, argv);
}

void *sys_sbrk(void)
{
    ptrdiff_t incr = 0;
    assert(!argint(0, (int *)&incr), "err fetching incr");
    return sbrk(incr);
}

/*Process relationships*/

pid_t sys_getppid(void)
{
    return getppid();
}

/*Memery management*/

int sys_getpagesize(void)
{
    return getpagesize();
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
