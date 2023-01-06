#pragma once

#include <arch/mmu.h>
#include <lib/types.h>
#include <sys/sched.h>
#include <locks/atomic.h>
#include <arch/context.h>
#include <arch/i386/lapic.h>

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
    thread_t    *current;
    sched_queue_t*ready_queue;
    proc_t *proc;
} cpu_t;

extern cpu_t cpus[NCPU];
extern volatile int ncpu;
extern atomic_t cpus_online;
extern cpu_t *bootstrap_cpu; 


extern cpu_t *cpu asm("%gs:0"); //local processor
extern thread_t *current asm("%gs:4");  //currently running thread
extern sched_queue_t*ready_queue asm("%gs:8");  //local cpu's ready queue
extern proc_t *proc asm("%gs:12");

#define CPU_RELAX() asm volatile("pause" ::: "memory")

void set_tss(uint32_t esp, int ss);
#define local_cpuid() (cpu->cpuid)
int ap_init(void);
int cpu_init(void);
void setupsegs(void);