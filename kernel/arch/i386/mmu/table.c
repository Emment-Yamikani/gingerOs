#include <printk.h>
#include <arch/i386/32bit.h>
#include <bits/errno.h>
#include <arch/system.h>
#include <arch/i386/paging.h>

extern uintptr_t _pgdir;

uintptr_t paging_switch(uintptr_t pgdir)
{
    uintptr_t oldpgdir = read_cr3();
    if (!pgdir)
    {
        write_cr3((read_cr3()));
        return oldpgdir;
    }
    write_cr3(PGROUND(pgdir));
    return oldpgdir;
}

void paging_switchkvm(void)
{
    write_cr3(_pgdir);
}