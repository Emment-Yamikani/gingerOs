#include <lime/module.h>
#include <lib/stddef.h>
#include <printk.h>

extern char __minit, __minit_end;
int modules_init()
{
    printk("Kernel: Loading builtin modules\n");
    /* Initalize built-in modules */
    void **f = (void **)&__minit;
    void **g = (void **)&__minit_end;

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