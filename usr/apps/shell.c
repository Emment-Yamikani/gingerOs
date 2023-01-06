#include <ginger.h>

void print_prompt(void)
{
    char cwd[256] = {0};
    getcwd(cwd, sizeof cwd);
    printf("[shell %s]# ", cwd);
}

#define SH_BUFSZ 512

char *read_line(void)
{
    int pos = 0;
    size_t buflen = SH_BUFSZ;
    char *cmdline = (char *)malloc(SH_BUFSZ);

    if (cmdline == NULL)
        panic("not enough memory to read command\n");

    print_prompt();

    while (1)
    {
        int c = getchar();
        if (c == -1 || c == '\n' || c == '\r')
        {
            cmdline[pos] = '\0';
            return cmdline;
        }
        else
            cmdline[pos] = c;
        pos++;
        if (pos > SH_BUFSZ)
        {
            buflen += SH_BUFSZ;
            cmdline = (char *)realloc(cmdline, buflen);
            if (cmdline == NULL)
                panic("not enough memory to read command\n");
        }
    }
    return NULL;
}

#define TOKS 2

static char *whitespace = " \a\n\t\r\v";
static char *pipeline = "<|>";

char **tokenize(const char *str)
{
    int toks = TOKS;
    char **tokens = calloc(TOKS, sizeof(char *));
    int pos = 0;
    char *token = strtok((char *)str, whitespace);

    while (token)
    {
        tokens[pos] = token;
        pos++;

        if (pos >= toks)
        {
            toks += TOKS;
            tokens = realloc(tokens, toks * sizeof(char *));
            if (tokens == NULL)
                panic("failed to reallocate memory");
        }
        token = strtok(NULL, whitespace);
    }

    tokens[pos] = NULL;
    return tokens;
}

int launch(char **args)
{
    pid_t pid = 0;
    int statloc = 0;
    if (args == NULL)
        return -1;
    printf("lauch %s: %d\n", args[0] ? args[0] : "(null)", statloc);

    if (pid = fork())
        pid = wait(&statloc);
    else if (pid < 0)
        panic("shell: error fork failed\n");
    else
    {
        execv(args[0], args);
        panic("shell: error failed to execute program: %s\n", args[0] ? args[0] : "(null)");
    }

    if (statloc)
        dprintf(2, "%s: exited with error: %d\n", args[0] ? args[0] : "(null)", statloc);
    return statloc;
}

static char *builtins[] = {
    "cd",
    "exit",
    "help",
    "--version",
    "-v",
    NULL,
};

static char *usage[] = {
    "cd [dir]",
    "exit",
    "help or \'help name\'",
    "shell --version or shell -v",
    "shell -v or shell --version",
    NULL,
};

static char *description[] = {
    "change directory.",
    "exit the shell.",
    "prints this list.",
    "print version information.",
    "print version information.",
    NULL,
};

int builtin_cd(char **);
int builtin_exit(char **);
int builtin_help(char **);
int builtin_version(char **);

static int (*builtin_cmd[])(char **) = {
    builtin_cd,
    builtin_exit,
    builtin_help,
    builtin_version,
    builtin_version,
    NULL,
};

int execute_builtin(char **args)
{
    for (int i = 0; i < NELEM(builtins) && builtins[i]; ++i)
        if (!strncmp(args[0], builtins[i], strlen(args[0])))
            return (builtin_cmd[i])(args);
    return -1;
}

int main(int argc, char *argv[], char *envp[])
{
    int err = 0;
    char *cmdline = NULL;
    char **args = NULL;

    printf("\nshell running\n");

    while ((cmdline = read_line()))
    {
        args = tokenize(cmdline);
        if (!execute_builtin(args))
            continue;
        else
            launch(args);

        free(args);
        free(cmdline);
    }

    exit(1);

    return 0xC0BECABE;
}

int builtin_cd(char **args)
{
    int err = 0;
    if (args == NULL)
        return -1;
    if (err = chdir(args[1]))
        dprintf(2, "shell: cd: %s: No such file or directory\n", args[1]);
    return 0;
}

int builtin_exit(char **args)
{
    if (args == NULL)
        return -1;
    exit(0);
    return -2;
}

int builtin_help(char **args)
{
    if (args == NULL)
        return -1;
    int i = 0;
    if (args[1])
    {
        printf("[Command]\t[Usage]\t\t\t[Description]\n\n");
        foreach (arg, &args[1])
            for (int i = 0; i < NELEM(builtins) && builtins[i]; ++i)
                if (!strncmp(arg, builtins[i], strlen(arg)))
                    printf("%s:\t%s:\t%s\n", arg, usage[i], description[i]);
        return 0;
    }
    else
    {
        printf("%s", "ginger shell, version 0.0.0\n"
                     "These shell commands are defined internally. Type \'help\' to see this list.\n"
                     "Type \'help name\' to find out more about the function \'name\'.\n"
                     "For info on others functions  not in this list use \'man [-function].\n");
        printf("\n[Command]\t[Usage]\t\t\t[Description]\n\n");
        foreach (cmdline, builtins)
            printf("%s:\t%s\t%s\n", cmdline, usage[i - 1], description[i++]);
    }
    return 0;
}

int builtin_version(char **args)
{
    if (args == NULL)
        return -1;
    printf("ginger shell, version 0.0.0\n"
           "Copyright (C) 2023 Neronbolt Corp.\n"
           "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n"
           "\nThis is free software; you are free to change and redistribute it.\n"
           "There is NO WARRANTY, to the extent permitted by law.\n");
    return 0;
}