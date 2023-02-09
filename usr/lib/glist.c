#include <glist.h>
#include <bits/errno.h>
#include <stddef.h>
#include <ginger.h>


//#include <sync/spinlock.h>

int glist_init(glist_t *l, glist_t **ref)
{
    int alloc = !l;

    if (alloc)
    {
        if (!ref)
            return -EINVAL;

        if (!(l = (typeof(l))malloc(sizeof *l)))
            return -ENOMEM;
    }

    memset(l, 0, sizeof *l);
    
    if (ref)
        *ref = l;

    return 0;
}

void glist_free(glist_t *l)
{
    if (!l)
    {
        printf("glist_free: no list");
        exit(1);
    }
    *l = (glist_t){0};
    free(l);
}

void glist_lock(glist_t *l)
{
    if (!l)
    {
            printf("glist_lock: no list");
            exit(1);
    }
    //spinlock_enter(&l->guard);
}

void glist_unlock(glist_t *l)
{
    if (!l)
    {
            printf("glist_lock: no list");
            exit(1);
    }
    //spinlock_exit(&l->guard);
}

int glist_add(glist_t *l, void *data)
{
    if (!l)
    {
        printf("glist_add: no list");
        exit(1);
    }

    glist_node_t *node = (typeof(node))malloc(sizeof *node);

    if (!node)
        return-ENOMEM;

    node->next = NULL;
    node->prev = NULL;
    node->ptr = data;

    glist_lock(l);

    if (!l->head)
        l->head = node;
    else
    {
        l->tail->next = node;
        node->prev = l->tail;
    }

    l->tail = node;

    ++l->count;

    glist_unlock(l);

    return 0;
}

void *glist_get(glist_t *glist)
{
    void *data = NULL;
    glist_node_t *node = NULL;

    if (!glist)
    {
        printf("glist_get: null glist\n");
        exit(1);
    }
    
    glist_lock(glist);

    if (!glist->head)
    {
        glist_unlock(glist);
        return NULL;
    }

    if ((glist->head = glist->head->next))
        glist->head->prev = NULL;
    
    --glist->count;

    data = node->ptr;

    free(node);

    glist_unlock(glist);

    return data;
}

glist_node_t *glist_lookup(glist_t *l, void *data)
{
    if (!l)
    {
        printf("glist_lookup: no list");
        exit(1);
    }

    forlinked(tmp, l->head, tmp->next)
    {
        if (tmp->ptr == data)
            return tmp;
    }

    return NULL;
}


int glist_del(glist_t *l, void *data)
{
    if (!l)
    {
        printf("glist_del: no list");
        exit(1);
    }

    glist_node_t *node = glist_lookup(l, data);
    glist_remove(l, node);

    return 0;
}


void glist_remove(glist_t *l, glist_node_t *node)
{
    if (!l)
    {
        printf("glist_remove: no list");
        exit(1);
    }

    if (!node)
        return;
    
    glist_lock(l);
    if (!glist_contains(l, node))
    {
        glist_unlock(l);
        return;
    }

    if (node->prev)
        node->prev->next = node->next;
    if (node->next)
        node->next->prev = node->prev;
    if (l->head == node)
        l->head = node->next;
    if (l->tail == node)
        l->tail = node->prev;

    --l->count;
    free(node);

    glist_unlock(l);
}

int glist_contains(glist_t *l, glist_node_t *n)
{
    if (!l)
    {
        printf("glist_contains: no list");
        exit(1);
    }
    
    forlinked(tmp, l->head, tmp->next)
    {
        if (tmp == n)
            return 1;
    }

    return 0;
}