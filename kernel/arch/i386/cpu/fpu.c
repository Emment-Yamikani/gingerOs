#include <arch/i386/cpu.h>
#include <arch/system.h>
#include <sys/thread.h>
#include <mm/kalloc.h>
#include <lib/string.h>

void fpu_enable(void)
{
    asm volatile ("clts");
    write_cr0((read_cr0() & ~(CR0_EM)) | CR0_MP);
}

void fpu_disable(void)
{
    write_cr0(read_cr0() | CR0_EM);
}

void fpu_init(void)
{
    asm volatile ("fninit");
}

void fpu_save(void)
{
    asm volatile("fxsave (%0)"::"r"(fpu_ctx):"memory");
}

void fpu_restore(void)
{
    asm volatile("fxrstor (%0)"::"r"(fpu_ctx):"memory");
}

void fpu_intr(void)
{
    fpu_enable();
    current_lock();

    if (fpu_thread == NULL) {
        fpu_init();
        current->fpu_enabled = 1;
    }else if (current != fpu_thread) {
        thread_lock(fpu_thread);
        if (fpu_thread->fpu_ctx == NULL) {
            while (fpu_thread->fpu_ctx == NULL) {
                if ((fpu_thread->fpu_ctx = kmalloc(512)) == NULL) {
                    thread_unlock(fpu_thread);
                    current_unlock();
                    fpu_disable();

                    /**
                     * Perhaps wait for memory manager,
                     * instead of doing a timed-sleep with the hope,
                     * that when we wakeup some memory might be available*/
                    thread_yield();

                    fpu_enable();
                    current_lock();
                    thread_lock(fpu_thread);
                }
            }
        }

        fpu_save();
        memcpy(fpu_thread->fpu_ctx, fpu_ctx, 512);
        thread_unlock(fpu_thread);

        if (current->fpu_enabled) {
            memcpy(fpu_ctx, current->fpu_ctx, 512);
            fpu_restore();
        } else {
            fpu_init();
            current->fpu_enabled = 1;
        }

    }

    fpu_thread = current;
    current_unlock();
}