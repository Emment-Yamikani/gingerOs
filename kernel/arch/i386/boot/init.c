#include <sys/system.h>
#include <lib/stddef.h>

extern char __einit, __einit_end;
extern char __efini, __efini_end;

int __early_init(void)
{
    void **f = (void **)&__einit;
    void **g = (void **)&__einit_end;

    size_t nr = (g - f);

    for (size_t i = 0; i < nr; ++i)
    {
        if (f[i])
        {
            ((int (*)())f[i])();
        }
    }

    return 0;
}

int __early_fini(void)
{
    void **f = (void **)&__efini;
    void **g = (void **)&__efini_end;

    size_t nr = (g - f);

    for (size_t i = 0; i < nr; ++i)
    {
        if (f[i])
        {
            ((int (*)())f[i])();
        }
    }

    return 0;
}