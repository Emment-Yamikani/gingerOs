#pragma once

#include <ds/queue.h>
#include <lib/stddef.h>
#include <lib/stdint.h>
#include <fs/fs.h>
#include <locks/spinlock.h>

#define VM_READ         0x000001    /* Pages can be read from. */
#define VM_WRITE        0x000002    /* Pages can be written to. */
#define VM_EXEC         0x000004    /* Pages can be executed. */
#define VM_SHARED       0x000008    /* Pages are shared. */
#define VM_MAYREAD      0x000010    /* The VM_READ flag can be set. */
#define VM_MAYWRITE     0x000020    /* The VM_WRITE flag can be set. */
#define VM_MAYEXEC      0x000040    /* The VM_EXEC flag can be set. */
#define VM_MAYSHARE     0x000080    /* The VM_SHARE flag can be set. */

#define VM_GROWSDOWN    0x000100    /* The area can grow downward. */
#define VM_GROWSUP      0x000200    /* The area can grow upward. */

#define VM_SHM          0x000400    /* The area is used for shared memory. */
#define VM_DENYWRITE    0x000800    /* The area maps an unwritable file. */
#define VM_EXECUTABLE   0x001000    /* The area maps an executable file. */
#define VM_LOCKED       0x002000    /* The pages in this area are locked. */
#define VM_IO           0x004000    /* The area maps a device's I/O space. */
#define VM_SEQ_READ     0x008000    /* The pages seem to be accessed sequentially. */
#define VM_RAND_READ    0x010000    /* The pages seem to be accessed randomly. */

#define VM_DONTCOPY     0x020000    /* This area must not be copied on fork(). */
#define VM_DONTEXPAND   0x040000    /* This area cannot grow via mremap(). */
#define VM_RESERVED     0x080000    /* This area must not be swapped out. */
#define VM_ACCOUNT      0x100000    /* This area is an accounted VM object. */
#define VM_HUGETLB      0x200000    /* This area uses hugetlb pages. */
#define VM_NONLINEAR    0x400000    /* This area is a nonlinear mapping.; */

struct shm;

typedef struct vmr
{
    atomic_t refs;    // number of references to this memory region
    int oflags;       // access flags
    int vflags;       // paging flags
    size_t size;      // size of this memory region
    uintptr_t paddr;  // if non-null, is the physical memory base address
    uintptr_t vaddr;  // if non-null, is the virtual memory base address
    inode_t *file;    // file pointer, if this region refers to  memory mapped file
    off_t file_pos;   // position in file
    off_t mmap_pos;   // position in memory map
    void *priv;       // private data

    struct vmr *prev; // next virtual memory region in this list, sorted by 'vaddr' address
    struct vmr *next; // next virtual memory region in this list, sorted by 'vaddr' address

    struct shm *shm;     // memory map to which this region belongs
    queue_node_t *qnode; // gain quick access to the queue holding this node
    spinlock_t *lock;      // virtual memory region mutex
} vmr_t;

typedef struct shm
{
    uintptr_t pgdir;        // page directory
    spinlock_t *pgdir_lock; // page directory lock
    size_t available_size;  // available address space

    size_t code_size;      // code size
    uintptr_t code_start;  // start of code segment
    size_t data_size;      // data size
    uintptr_t data_start;  // start of data segment
    size_t env_size;       // size of env variables
    uintptr_t env_start;   // start of env variables
    size_t arg_size;       // size of arg variable
    uintptr_t arg_start;   // start of arg variables
    size_t brk_size;       // size of brk
    uintptr_t brk_start;   // start of brk
    uintptr_t stack_start; // start of stack

    atomic_t refs;      // # of references
    atomic_t nvmr;      // # of vmrs
    atomic_t nmpgs;     // # of mapped pages
    atomic_t npgfaults; // # of page faults

    vmr_t *vmr_head, *vmr_tail; // list of vmr
    queue_t *vmrs;   // queue of virtual memory regions
    spinlock_t *lock;  // shm struct mutex
} shm_t;

#define vmr_assert(vmr) assert(vmr, "no vmr")
#define vmr_assert_lock(vmr) { vmr_assert(vmr); spin_assert_lock(vmr->lock); }
#define vmr_lock(vmr) { vmr_assert(vmr); spin_lock(vmr->lock); }
#define vmr_unlock(vmr) { vmr_assert(vmr); spin_unlock(vmr->lock); }

void vmr_free(vmr_t *);
int vmr_alloc(vmr_t **);

#define shm_assert(shm) assert(shm, "no shm")
#define shm_assert_pgdir_lock(shm) { shm_assert(shm); spin_assert_lock(shm->pgdir_lock); }
#define shm_pgdir_lock(shm) { shm_assert(shm); spin_lock(shm->pgdir_lock); }
#define shm_pgdir_unlock(shm) { shm_assert(shm); spin_unlock(shm->pgdir_lock); }
#define shm_assert_lock(shm) {shm_assert(shm); spin_assert_lock(shm->lock);}
#define shm_lock(shm) { shm_assert(shm); spin_lock(shm->lock); }
#define shm_try_lock(shm) spin_try_lock(shm->lock)
#define shm_unlock(shm) { shm_assert(shm); spin_unlock(shm->lock); }

int shm_remove_all(shm_t *);
void shm_free(shm_t *);
int shm_alloc(shm_t **);
int shm_copy(shm_t *, shm_t *);
int shm_map(shm_t *, vmr_t *);
int shm_unmap(shm_t *, vmr_t *);
int shm_unmap_addrspace(shm_t *);
int shm_remove(shm_t *, vmr_t *);
int shm_contains(shm_t *, vmr_t *);
vmr_t *shm_lookup(shm_t *, uintptr_t);
int shm_alloc_stack(shm_t *, vmr_t **);