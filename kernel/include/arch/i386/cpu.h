#pragma once

#include <arch/mmu.h>
#include <lib/types.h>
#include <sys/sched.h>
#include <locks/atomic.h>
#include <arch/context.h>
#include <arch/i386/lapic.h>
#include <sys/system.h>

#define NCPU    8
#define MPSTACK 0x8000

typedef struct cpu
{
    int         ncli;
    int         cpuid;
    int         intena;
    int         enabled;
    atomic_t    online;
    atomic_t    timer_ticks;
    uint64_t    feartures;
    context_t   *context;

    tss_t       tss;
    systable_t  gdtptr;
    seg_desc_t  gdt[NSEG];

    struct cpu  *cpu;
    struct proc *proc;
    void        *fpu_ctx;
    thread_t    *current;
    thread_t    *fpu_thread;
    sched_queue_t*ready_queue;
} cpu_t;

extern cpu_t cpus[NCPU];
extern volatile int ncpu;
extern char fpus[NCPU][512];
extern atomic_t cpus_online;
extern cpu_t *bootstrap_cpu; 

extern cpu_t *cpu asm("%gs:0"); //local processor
extern proc_t *proc asm("%gs:4");
extern void *fpu_ctx asm("%gs:8");
extern thread_t *current asm("%gs:12");  //currently running thread
extern thread_t *fpu_thread asm("%gs:16");
extern sched_queue_t*ready_queue asm("%gs:20");  //local cpu's ready queue

#define CPU_RELAX() asm volatile("pause" ::: "memory")

void set_tss(uint32_t esp, int ss);
#define local_cpuid() (cpu->cpuid)
int ap_init(void);
int cpu_init(void);
void setupsegs(void);


#define CR0_MP  (_BS(1))
#define CR0_EM  (_BS(2))
#define CR0_TS  (_BS(3))

extern void fpu_enable(void);
extern void fpu_disable(void);
extern void fpu_init(void);
extern void fpu_save(void);
extern void fpu_restore(void);
extern void fpu_intr(void);