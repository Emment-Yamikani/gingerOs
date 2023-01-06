#include <printk.h>
#include <locks/spinlock.h>
#include <lib/stddef.h>
#include <lib/stdint.h>
#include <bits/errno.h>
#include <mm/vmm.h>
#include <sys/system.h>
#include <arch/i386/paging.h>
#include <lib/string.h>
#include <lime/assert.h>

extern void _kernel_start();
extern void _kernel_end();

static spinlock_t *vmm_spinlock = SPINLOCK_NEW("vmm_spinlock");
static size_t used_virtual_mmsz = 0;

#define KVM_DEBUG 0

/// @brief get page size
/// @param  void
/// @return int
int getpagesize(void) { return PAGESZ; }

typedef struct node
{
    uint32_t        base; //start of region (zero if free)
    struct node *next;  //next region
    struct node *prev;  //prev region
    size_t          size;  //size of region in bytes
}node_t;

#define KHEAPBASE (VMA_HIGH(0x1000000))         //kernel heap base address.
#define KHEAPSZ   ((0xFE000000) - KHEAPBASE)    //size of kernel heap.

#define KHEAP_MAX_NODES ((int)KHEAPSZ / PAGESZ)  //maximum blocks that can be address.
#define KHEAP_NODES_ARRAY ((node_t *)KHEAPBASE)  //array of memory nodes.
#define KHEAP_NODES_ARRAY_SZ (KHEAP_MAX_NODES * sizeof (node_t)) // sizeof node array

struct vmrx
{
    int v_refs;        // refresnce count
    int v_flags;       // flags associated with this region of virtual memory
    size_t v_size;     // size of memory region
    uintptr_t v_base;  // virtual base address
    uintptr_t v_paddr; // physical address
} kheap_vmrs[] =
{
    [0] = {.v_base = VMA_HIGH(0x100000), .v_size = 0, .v_refs = 1},
    [1] = {.v_base = KHEAPBASE, .v_size = KHEAP_NODES_ARRAY_SZ, .v_paddr = 0, .v_flags = (VM_PWT | VM_KRW | VM_PCD), .v_refs = 1},
    [2] = {.v_base = MMAP_DEVADDR, .v_refs =1, .v_flags = (VM_PWT | VM_KRW | VM_PCD), .v_paddr = MMAP_DEVADDR}
};

static node_t *usedvmr_head = NULL;
static node_t *usedvmr_tail = NULL;

static node_t *freevmr_head = NULL;
static node_t *freevmr_tail = NULL;

static node_t *freenodes_head = NULL;
static node_t *freenodes_tail = NULL;

static node_t nodes[KHEAP_MAX_NODES];

static int can_merge_left(node_t *node, node_t *left)
{
    assert(node, "no node");
    assert(left, "no left");
    return (left->base + left->size) == node->base;
}

static int can_merge_right(node_t *node, node_t *right)
{
    assert(node, "no node");
    assert(right, "no right");
    return (node->base + node->size) == right->base;
}

static int can_merge_both(node_t *left, node_t *node, node_t *right)
{
    return can_merge_left(node, left) && can_merge_right(node, right);
}

static node_t *freenodes_get(void)
{
    if (!freenodes_head)
        return NULL;
    node_t *node = freenodes_head;
    freenodes_head = node->next;
    if (!freenodes_head)
        freenodes_tail = NULL;
    *node = (node_t){0};
    return node;
}

static void freenodes_put(node_t *node)
{
    assert(node, "no node");
    assert(node->size, "invalid memory region size");
    assert(node->base >= KHEAPBASE, "invalid memory region base address");
    if (!freenodes_head)
        freenodes_head = node;
    if (freenodes_tail)
        freenodes_tail->next = node;
    node->next = NULL;
    node->prev = freenodes_tail;
    freenodes_tail = node;
}

static void merge_left(node_t *node, node_t *left)
{
    assert(node, "no node");
    assert(left, "no left");
    if (!can_merge_left(node, left))
        panic("node[%p: %dkib] can't merge with left[%p: %dkib]\n",
            node->base, node->size / 1024, left->base, left->size / 1024);
    left->size += node->size;
    freenodes_put(node);
}

static void merge_right(node_t *node, node_t *right)
{
    assert(node, "no node");
    assert(right, "no right");
    if (!can_merge_right(node, right))
        panic("node[%p: %dkib] can't merge with right[%p: %dkib]\n",
            node->base, node->size / 1024, right->base, right->size / 1024);
    right->base = right->base - node->size;
    right->size += node->size;
    freenodes_put(node);
}

static void concatenate(node_t *left, node_t *node, node_t *right)
{
    if (!can_merge_both(left, node, right))
        panic("node[%p:%dB] can't merge left[%p:%dB] and right[%p:%dB]\n",
            node->base, node->size, left->base, left->size, right->base, right->size);
    left->size += (node->size + right->size);
    if (right->next)
        right->next->prev = left;
    left->next = right->next;
    if (!right->next)
        freevmr_tail = left;
    freenodes_put(node);
    freenodes_put(right);
}

static void freevmr_linkto_left(node_t *node, node_t *left)
{
    assert(node, "no node");
    assert(left, "no left");

    node->next = left->next;
    if (left->next)
        left->next->prev = node;
    else
        freevmr_tail = node;
    left->next = node;
    node->prev = left;
}

static void freevmr_linkto_right(node_t *node, node_t *right)
{
    assert(node, "no node");
    assert(right, "no right");

    node->prev = right->prev;
    if (right->prev)
        right->prev->next = node;
    else
        freevmr_head = node;
    right->prev = node;
    node->next = right;
}

static void freevmr_put(node_t *node)
{
    assert(node, "no node");
    assert(node->size, "invalid memory region size");
    assert(node->base >= KHEAPBASE, "invalid memory region base address");

    node_t *tmp = NULL, *right = freevmr_head, *left = NULL;
    
    if (!freevmr_head)
    {
        freevmr_head = node;
        freevmr_tail = node;
        return;
    }

    
    for (tmp = right; tmp && (right->base < node->base); tmp = right = tmp->next)
    {
        left = right;
    }

    if (left && right)
    {
        #if KVM_DEBUG
            printk("left and right\n");
        #endif
        if (can_merge_both(left, node, right))
        {
            #if KVM_DEBUG
                printk("can merge with both L[%p : %dkib] : [%p : %dkib] : R[%p : %dkib]\n",
                        left->base, left->size / 1024, node->base, node->size / 1024, right->base, right->size / 1024);
            #endif
            concatenate(left, node, right);
        }
        else if (can_merge_left(node, left))
        {
            #if KVM_DEBUG
                printk("can merge with left L[%p : %dkib] : [%p : %dkib] : R[%p : %dkib]\n",
                        left->base, left->size / 1024, node->base, node->size / 1024, right->base, right->size / 1024);
            #endif
            merge_left(node, left);
        }
        else if (can_merge_right(node, right))
        {
            #if KVM_DEBUG
                printk("can merge with right L[%p : %dkib] : [%p : %dkib] : R[%p : %dkib]\n",
                        left->base, left->size / 1024, node->base, node->size / 1024, right->base, right->size / 1024);
            #endif
            merge_right(node, right);
        }
        else
        {
            #if KVM_DEBUG
                printk("can not merge with any L[%p : %dkib] : [%p : %dkib] : R[%p : %dkib]\n",
                        left->base, left->size / 1024, node->base, node->size / 1024, right->base, right->size / 1024);
            #endif
            freevmr_linkto_left(node, left);
            freevmr_linkto_right(node, right);
        }
    }
    else if (left)
    {
        #if KVM_DEBUG
            printk("only has left L[%p : %dkib] : [%p : %dkib]\n",
                left->base, left->size / 1024, node->base, node->size / 1024);
        #endif
        if (can_merge_left(node, left))
        {
            #if KVM_DEBUG
                printk("can merge left L[%p : %dkib] : [%p : %dkib]\n",
                        left->base, left->size / 1024, node->base, node->size / 1024);
            #endif
            merge_left(node, left);
        }
        else
        {
            #if KVM_DEBUG
                printk("can not merge left L[%p : %dkib] : [%p : %dkib]\n",
                        left->base, left->size / 1024, node->base, node->size / 1024);
            #endif
            freevmr_linkto_left(node, left);
        }
    }
    else if (right)
    {
        #if KVM_DEBUG
            printk("only has right [%p : %dkib] : R[%p : %dkib]\n",
                    node->base, node->size / 1024, right->base, right->size / 1024);
        #endif
        if (can_merge_right(node, right))
        {
            #if KVM_DEBUG
                printk("can merge right [%p : %dkib] : R[%p : %dkib]\n",
                    node->base, node->size / 1024, right->base, right->size / 1024);
            #endif
            merge_right(node, right);
        }
        else
        {
            #if KVM_DEBUG
                printk("can not merge right [%p : %dkib] : R[%p : %dkib]\n",
                    node->base, node->size / 1024, right->base, right->size / 1024);
            #endif
            freevmr_linkto_right(node, right);
        }
    }
    else
        panic("impossible constraint\n");
    
}

static node_t *freevmr_get(size_t sz)
{
    assert(sz, "invalid memory size request");
    assert(!(sz & PAGEMASK), "invalid size, must be page aligned");
    node_t *node = freevmr_head;

    if (!node)
        return NULL;
    
    for (; node ; node = node->next)
        if (node->size >= sz)
            break;

    if (node)
    {
        if (node->prev)
            node->prev->next = node->next;

        if (node->next)
            node->next->prev = node->prev;

        if (freevmr_head == node)
            freevmr_head = node->next;

        if (freevmr_tail == node)
            freevmr_tail = node->prev;

        node->next = NULL;
        node->prev = NULL;
    }

    return node;
}

static void usedvmr_put(node_t *node)
{
    assert(node, "no node");
    assert(node->size, "invalid memory region size");
    assert(node->base >= KHEAPBASE, "invalid memory region base address");
    if (!usedvmr_head)
        usedvmr_head = node;
    if (usedvmr_tail)
        usedvmr_tail->next = node;
    node->next = NULL;
    node->prev = usedvmr_tail;
    usedvmr_tail = node;
}

static node_t *usedvmr_lookup(uintptr_t base)
{
    assert(base, "no memory region base address specified");
    node_t *node = usedvmr_head;

    if (!node)
        return NULL;
        
    for (; node ; node = node->next)
        if (node->base == base)
            break;

    if (node)
    {
        if (node->prev)
            node->prev->next = node->next;
        
        if (node->next)
            node->next->prev = node->prev;
        
        if (usedvmr_head == node)
            usedvmr_head = node->next;
        
        if (usedvmr_tail == node)
            usedvmr_tail = node->prev;

        node->next = NULL;
        node->prev = NULL;
    }
    return node;
}

static uintptr_t vmm_alloc(size_t sz)
{
    uintptr_t addr = 0;
    node_t *split = NULL, *node = NULL;

    spin_lock(vmm_spinlock);
    
    split = freevmr_get(sz);
    
    if (!split)
    {
        #if KVM_DEBUG
        klog(KLOG_FAIL, "couldn't allocate %dkib, no free virtual memory available\n", sz / 1024);
        #endif
        spin_unlock(vmm_spinlock);
        return 0;
    }

    if (split->size > sz)
    {
        node = freenodes_get();
        assert(node, "no free nodes left");
        node->base = split->base;
        node->size = sz;
        split->base += sz;
        split->size -= sz;
        freevmr_put(split);
    }
    else if (split->size < sz)
        panic("impossible contraint with size\n");
    else
        node = split;
    
    used_virtual_mmsz += sz;
    addr = node->base;
    usedvmr_put(node);
    
    #if KVM_DEBUG
    klog(KLOG_OK, "alocated %dKib @ %p\n", sz / 1024, addr);
    #endif

    spin_unlock(vmm_spinlock);
    return addr;
}

static void vmm_free(uintptr_t base)
{
    node_t *node = NULL;
    assert(base, "no memory region base address specified");

    spin_lock(vmm_spinlock);

    if (!(node = usedvmr_lookup(base)))
        panic("%p was not allocated before\n", base);
    
    #if KVM_DEBUG
        printk("freeing %p\n", base);
    #endif
    
    used_virtual_mmsz -= node->size;
    freevmr_put(node);
    
    #if KVM_DEBUG
    klog(KLOG_OK, "freed %dkib @ %p\n", node->size / 1024, node->base);
    #endif
    
    spin_unlock(vmm_spinlock);
}

static size_t vmm_getfreesize(void)
{
    spin_lock(vmm_spinlock);
    size_t size = (((size_t)KHEAPSZ) - used_virtual_mmsz);
    spin_unlock(vmm_spinlock);
    return size / 1024;
}

static size_t vmm_getinuse(void)
{
    spin_lock(vmm_spinlock);
    size_t size = used_virtual_mmsz;
    spin_unlock(vmm_spinlock);
    return size / 1024;
}

static int vmm_init(void)
{
    klog_init(KLOG_INIT, "virtual memory manager");
    int i =0;
    memset(nodes, 0, sizeof nodes);
    for (; i < (KHEAP_MAX_NODES -1); ++i)
    {
        nodes[i].prev = nodes + (i - 1);
        nodes[i].next = (nodes + (i + 1));
    }

    nodes[0].prev = NULL;
    nodes[i].next = NULL;
    nodes[i].prev = &nodes[i -1]; 

    freenodes_head = nodes;
    freenodes_tail = &nodes[i];
    
    node_t *node = NULL;
    *(node = freenodes_get()) = (node_t)
    {
        .base = KHEAPBASE,
        .size = KHEAPSZ
    };
    
    freevmr_put(node);

    klog_init(KLOG_OK, "virtual memory manager");
    return 0;
}

#include <mm/pmm.h>
void memory_usage(void)
{
    printk("\t\t\t\e[0;06mMEMORY USAGE INFO\e[0m\n");
    printk("\t\t\t\e[0;015mPhysical Memory\e[0m\nFree  : \e[0;012m%8dKB\e[0m\nIn use: \e[0;04m%8dKB\e[0m\n\n", pmman.mem_free(), pmman.mem_used());
    printk("\t\t\t\e[0;015mVirtual Memory\e[0m\nFree  : \e[0;012m%8dKB\e[0m\nIn use: \e[0;04m%8dKB\e[0m\n\n", vmman.getfreesize(), vmman.getinuse());
}

struct vmman vmman =
{
    .init = vmm_init,
    .alloc = vmm_alloc,
    .free = vmm_free,
    .getfreesize = vmm_getfreesize,
    .getinuse = vmm_getinuse,
};