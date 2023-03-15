#include <arch/sys/uthread.h>
#include <bits/errno.h>
#include <lime/assert.h>
#include <printk.h>
#include <sys/system.h>
#include <mm/kalloc.h>
#include <lib/string.h>

int arch_uthread_execve(uintptr_t *ustack, const char **__argp, const char **__envp)
{
    int argc = 0, envc = 0, tmp_argc = 0, tmp_envc = 0;
    char **argp = NULL, **envp = NULL, *stack = (char *)*ustack;

    assert(ustack, "no thread");

    if (__envp) foreach (env, __envp) envc++;
    if (__argp) foreach (arg, __argp) argc++;

    if ((envp = kcalloc(envc + 1, sizeof(char *))) == NULL)
        return -ENOMEM;

    if ((argp = kcalloc(argc + 1, sizeof(char *))) == NULL)
    {
        kfree(envp);
        return -ENOMEM;
    }

    tmp_envc = envc;
    tmp_argc = argc;

    envp[tmp_envc--] = NULL;
    argp[tmp_argc--] = NULL;

    if (__envp) foreach (env, __envp)
    {
        int len = strlen(__envp[tmp_envc]) + 1;
        stack -= len;
        safestrcpy(stack, __envp[tmp_envc], len);
        envp[envc--] = stack;
    }

    if (__argp) foreach (arg, __argp)
    {
        int len = strlen(__argp[tmp_argc]) + 1;
        stack -= len;
        safestrcpy(stack, __argp[tmp_argc], len);
        argp[tmp_argc--] = stack;
    }

    stack -= (envc + 1) * sizeof(char *);
    memcpy(stack, envp, ((envc + 1) * sizeof(char *)));

    kfree(envp);
    envp = (char **)stack;

    stack -= (argc + 1) * sizeof(char *);
    memcpy(stack, argp, ((argc + 1) * sizeof(char *)));
    kfree(argp);

    argp = (char **)stack;
    uint32_t *stack_top = (uint32_t *)stack;

    /**
     * @brief exec() starts running a program image as main(int argc, char *argv[]);*/
    *--stack_top = (uint32_t)envp;       // push poiter environment variables
    *--stack_top = (uint32_t)argp;       // push poiter argument variables
    *--stack_top = (uint32_t)argc;       // push argc
    *--stack_top = (uint32_t)0xDEADDEAD; // push dummy return address

    *ustack = (uintptr_t)stack_top;
    return 0;
}

char ***arch_execve_cpy(char *_argp[], char *_envp[])
{
    int argc = 0;
    int envc = 0;
    char **argp = NULL;
    char **envp = NULL;
    char ***ptr = kcalloc(3, sizeof(char **));

    if (_argp) foreach (token, _argp) argc++;

    if (_envp) foreach (token, _envp) envc++;

    argp = kcalloc(argc + 1, sizeof(char *));
    argp[argc] = NULL;
    
    envp = kcalloc(envc + 1, sizeof(char *));
    envp[envc] = NULL;

    if (_argp)
    {
        foreach (arg, _argp)
        {
            argc--;
            argp[argc] = strdup(_argp[argc]);
        }
    }

    if (_envp)
    {
        foreach (env, _envp)
        {
            envc--;
            envp[envc] = strdup(_envp[envc]);
        }
    }

    ptr[0] = argp;
    ptr[1] = envp;
    ptr[2] = NULL;

    return ptr;
}

void arch_exec_free_cpy(char ***arg_env)
{
    if (!arg_env) return;
    foreach (list, arg_env) foreach (arg, list) kfree(arg);
    kfree(arg_env);
}