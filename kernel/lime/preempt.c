#include <arch/mmu.h>
#include <arch/system.h>
#include <lime/assert.h>
#include <lime/preempt.h>
#include <arch/i386/cpu.h>

void pushcli(void)
{
    int intena = read_eflags() & FL_IF;
    cli();
    if (!cpu->ncli++ && intena)
        cpu->intena = intena;
}

void popcli(void)
{
    assert(!(read_eflags() & FL_IF), "popcli interruptable");
    assert(!(--cpu->ncli < 0), "popcli underflow");
    if (!cpu->ncli && cpu->intena)
        sti();
}