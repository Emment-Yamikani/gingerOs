#include <printk.h>
#include <ds/queue.h>
#include <mm/kalloc.h>
#include <bits/errno.h>
#include <lib/string.h>
#include <lime/string.h>

void queue_free(queue_t *q)
{
    queue_assert_lock(q);
    queue_flush(q);
    if (q->name && (q->flags & 1)) kfree(q->name);
    if (q->lock) spinlock_free(q->lock);
    if (q->flags & 1) kfree(q);
}

void queue_flush(queue_t *q)
{
    queue_assert_lock(q);
    if (spin_try_lock(q->lock))
        panic("caller not holding %s\n", q->lock->name);
    while (dequeue(q))
        ;
}

void *dequeue(queue_t *q)
{
    void *data = NULL;
    queue_node_t *node = NULL;

    queue_assert_lock(q);
    if (spin_try_lock(q->lock))
        panic("caller not holding %s\n", q->lock->name);

    if (!(node = q->head))
        return NULL;
    if (q->head == q->tail)
        q->head = q->tail = NULL;
    if ((q->head = node->next))
        q->head->prev = NULL;
    data = node->data;
    q->count--;

    kfree(node);
    return data;
}

queue_node_t *enqueue(queue_t *q, void *data)
{
    queue_node_t *node = NULL;

    queue_assert_lock(q);
    if (spin_try_lock(q->lock))
        panic("caller not holding %s\n", q->lock->name);

    if (!(node = __cast_to_type(node) kmalloc(sizeof *node)))
        return NULL;
    *node = (queue_node_t){0};

    if (!q->head)
        q->head = node;

    if (q->tail)
    {
        q->tail->next = node;
        node->prev = q->tail;
    }
    q->count++;
    q->tail = node;
    node->queue = q;
    node->data = data;
    return node;
}

int queue_count(queue_t *q)
{
    queue_assert_lock(q);
    if (spin_try_lock(q->lock))
        panic("caller not holding %s\n", q->lock->name);
    return q->count;
}

queue_node_t *queue_contains(queue_t *q, void *data)
{
    queue_assert_lock(q);
    if (spin_try_lock(q->lock))
        panic("caller not holding %s\n", q->lock->name);

    forlinked(node, q->head, node->next) if (node->data == data) return node;
    return NULL;
}

int queue_contains_node(queue_t *q, queue_node_t *node)
{
    queue_assert_lock(q);
    if (spin_try_lock(q->lock))
        panic("caller not holding %s\n", q->lock->name);
    
    forlinked(n, q->head, n->next) if (n == node) return 1;
    return 0;
}

int queue_new(const char *__name, queue_t **ref)
{
    int err = 0;
    queue_t *q = NULL;
    char *name = NULL;
    spinlock_t *lock = NULL;
    
    if (!__name || !ref)
        return -EINVAL;

    if (!(q = __cast_to_type(q) kmalloc(sizeof *q)))
    {
        err = -ENOMEM;
        goto error;
    }

    if (!(name = combine_strings(__name, "-queue")))
    {
        err = -ENOMEM;
        goto error;
    }

    if ((err = spinlock_init(NULL, name, &lock)))
        goto error;

    *q = (queue_t){0};
    
    q->lock = lock;
    q->name = name;
    q->flags = 1;

    *ref = q;
    return 0;

error:
    if (name)
        kfree(name);
    if (lock)
        spinlock_free(lock);
    if (q)
        kfree(q);
    printk("queue_new(): error = %d\n", err);
    return err;
}

int queue_remove_node(queue_t *q, queue_node_t *node)
{
    queue_assert_lock(q);
    assert(node, "no node");

    if (spin_try_lock(q->lock))
        panic("caller not holding %s\n", q->lock->name);

    if (!queue_contains_node(q, node))
        return -ENOENT;

    if (node->prev)
        node->prev->next = node->next;
    if (node->next)
        node->next->prev = node->prev;
    if (q->head == node)
        q->head = node->next;
    if (q->tail == node)
        q->tail = node->prev;

    q->count--;
    kfree(node);

    return 0;
}

int queue_remove(queue_t *q, void *data)
{
    queue_assert_lock(q);
    if (spin_try_lock(q->lock))
        panic("caller not holding %s\n", q->lock->name);
    queue_node_t *node = queue_contains(q, data);
    if (!node) return -ENOENT;
    return queue_remove_node(q, node);
}