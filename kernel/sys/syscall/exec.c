#include <mm/kalloc.h>
#include <mm/mmap.h>
#include <sys/binfmt.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/thread.h>
#include <arch/i386/cpu.h>
#include <arch/i386/paging.h>
#include <arch/sys/uthread.h>
#include <arch/system.h>

int argenv_copy(mmap_t *__mmap, const char *__argp[], const char *__envp[], char **argv[], int *pargc, char **envv[]);

int execve(const char *fn, const char *argp[], const char *envp[])
{
    int err = 0;
    char ***arg_envp = NULL;
    char *binary_path = NULL;
    context_t *swtch_ctx = NULL;
    mmap_t *new_mmap = NULL, *old_mmap = NULL;

    if (fn == NULL)
        return -EINVAL;

    if (current == NULL)
        return -EINVAL;

    if ((arg_envp = arch_execve_cpy((char **)argp, (char **)envp)) == NULL)
        return -ENOMEM;

    if ((binary_path = strdup(fn)) == NULL)
    {
        err = -ENOMEM;
        goto error;
    }

    proc_lock(proc);
    current_lock();
    old_mmap = current_mmap();

    if (old_mmap == NULL)
    {
        current_unlock();
        proc_unlock(proc);
        err = -EINVAL;
        goto error;
    }

    mmap_lock(old_mmap);

    if ((err = mmap_alloc(&new_mmap)))
    {
        mmap_unlock(old_mmap);
        current_unlock();
        proc_unlock(proc);
        goto error;
    }

    mmap_lock(new_mmap);

    /**
     * We switch to the new address space
     * So that we can return to old image,
     * if at all we fail to succeed*/
    mmap_switch(new_mmap);

    if ((err = binfmt_load(binary_path, proc, NULL)))
    {
        mmap_switch(old_mmap);
        mmap_unlock(new_mmap);
        mmap_unlock(old_mmap);
        current_unlock();
        proc_unlock(proc);
        goto error;
    }

    if ((err = thread_execve(proc, current, (void *)proc->entry, (const char **)arg_envp[0], (const char **)arg_envp[1])))
    {
        mmap_switch(old_mmap);
        mmap_unlock(new_mmap);
        mmap_unlock(old_mmap);
        current_unlock();
        proc_unlock(proc);
        goto error;
    }

    arch_exec_free_cpy(arg_envp);

    atomic_or(&proc->flags, PROC_EXECED);
    proc->name = binary_path;
    signals_cancel(proc);

    mmap_switch(old_mmap);
    mmap_free(old_mmap);
    mmap_switch(new_mmap);

    proc_unlock(proc);

    mmap_unlock(new_mmap);

    current->t_state = T_READY;

    swtch(&swtch_ctx, cpu->context);

    return 0;
error:
    if (binary_path)
        kfree(binary_path);
    if (arg_envp)
        arch_exec_free_cpy(arg_envp);
    if (new_mmap)
        mmap_free(new_mmap);
    printk("Error %s() failed with error= %d\n", __func__, err);
    return err;
}

int argenv_copy(mmap_t *__mmap, const char *__argp[], const char *__envp[], char **argv[], int *pargc, char **envv[])
{
    int err = 0, index = 0;
    int argc = 0, envc = 0;
    size_t argslen = 0, envslen = 0;
    char **argp = NULL, **envp = NULL;
    vmr_t *argvmr = NULL, *envvmr = NULL;
    char *arglist = NULL, *envlist = NULL;

    if (__mmap == NULL)
        return -EINVAL;

    if ((__argp && argv == NULL) || (__envp && envv == NULL))
        return -EINVAL;

    mmap_assert_locked(__mmap);

    if (pargc)
        *pargc = 0;

    if (__argp)
    {
        /**Count the command-line arguments.
         * Also keep the size memory region
         * needed to hold the argumments.
         */
        foreach (arg, __argp)
        {
            argc++;
            argslen += strlen(arg) + 1 + sizeof(char *);
        }
    }

    // align the size to PAGE alignment
    argslen = GET_BOUNDARY_SIZE(0, argslen);

    // If region ength is 0 then jump to setting 'arg_array'
    if (argslen == 0)
        goto arg_array;

    argc++;

    // allocate space for the arg_array and args
    if ((err = mmap_alloc_vmr(__mmap, argslen, PROT_RW, MAP_PRIVATE | MAP_DONTEXPAND, &argvmr)))
        goto error;

    // Page the region in
    if ((err = paging_mappages(argvmr->start, argslen, argvmr->vflags)))
        goto error;

    __mmap->arg = argvmr;
    arglist = (char *)argvmr->start;

    // allocate temoporal array to hold pointers to args
    if ((argp = kcalloc(argc, sizeof(char *))) == NULL)
    {
        err = -ENOMEM;
        goto error;
    }

    if (__argp)
    {
        // Do actual copyout of args
        foreach (arg, __argp)
        {
            ssize_t arglen = strlen(arg) + 1;
            safestrcpy(arglist, arg, arglen);
            argp[index++] = arglist;
            arglist += arglen;
        }
    }

    // copyout the array of arg pointers
    memcpy(arglist, argp, argc * sizeof(char *));
    // free temporal memory
    kfree(argp);

arg_array:
    if (argv)
        *argv = (char **)arglist;

    if (__envp)
    {
        /**Count the Environments.
         * Also keep the size memory region
         * needed to hold the Environments.
         */
        foreach (env, __envp)
        {
            envc++;
            envslen += strlen(env) + 1 + sizeof(char *);
        }
    }

    // align the size to PAGE alignment
    envslen = GET_BOUNDARY_SIZE(0, envslen);

    // If region ength is 0 then jump to setting 'env_array'
    if (envslen == 0)
        goto env_array;

    envc++;

    // allocate space for the env_array and envs
    if ((err = mmap_alloc_vmr(__mmap, envslen, PROT_RW, MAP_PRIVATE | MAP_DONTEXPAND, &envvmr)))
        goto error;

    // Page the region in
    if ((err = paging_mappages(envvmr->start, envslen, envvmr->vflags)))
        goto error;

    __mmap->env = envvmr;
    envlist = (char *)envvmr->start;

    // allocate temoporal array to hold pointers to envs
    index = 0;
    if ((envp = kcalloc(envc, sizeof(char *))) == NULL)
    {
        err = -ENOMEM;
        goto error;
    }

    if (__envp)
    {
        //  Do actual copyout of envs
        foreach (env, __envp)
        {
            ssize_t envlen = strlen(env) + 1;
            safestrcpy(envlist, env, envlen);
            envp[index++] = envlist;
            envlist += envlen;
        }
    }

    // copyout the array of env pointers
    memcpy(envlist, envp, envc * sizeof(char *));
    // free temporal memory
    kfree(envp);

env_array:
    if (envv)
        *envv = (char **)envlist;

    if (pargc)
        *pargc = --argc;

    return 0;
error:
    if (envp)
        kfree(envp);
    if (envvmr)
        mmap_remove(__mmap, envvmr);
    if (argp)
        kfree(argp);
    if (argvmr)
        mmap_remove(__mmap, argvmr);
    return err;
}

int execv(const char *path, const char *argp[])
{
    return execve(path, (const char **)argp, NULL);
}