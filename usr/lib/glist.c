#include <glist.h>
#include <bits/errno.h>
#include <stddef.h>
#include <ginger.h>

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
        return;
    *l = (glist_t){0};
    free(l);
}

int glist_add(glist_t *l, void *data)
{
    if (!l)
        return -EINVAL;

    glist_node_t *node = (typeof(node))malloc(sizeof *node);

    if (!node)
        return -ENOMEM;

    node->next = NULL;
    node->prev = NULL;
    node->ptr = data;

    if (!l->head)
        l->head = node;
    
    if (l->tail)
    {
        l->tail->next = node;
        node->prev = l->tail;
    }

    l->tail = node;
    ++l->count;
    return 0;
}

void *glist_get(glist_t *glist)
{
    void *data = NULL;
    glist_node_t *node = NULL;

    if (!glist)
        return NULL;

    if (!(node = glist->head))
        return NULL;

    if (glist->head == glist->tail)
        glist->head = glist->tail = NULL;
    if ((glist->head = node->next))
        glist->head->prev = NULL;

    --glist->count;

    data = node->ptr;

    free(node);

    return data;
}

glist_node_t *glist_lookup(glist_t *l, void *data)
{
    if (!l)
        return NULL;

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
        return -EINVAL;

    glist_node_t *node = glist_lookup(l, data);
    glist_remove(l, node);

    return 0;
}

void glist_remove(glist_t *l, glist_node_t *node)
{
    if (!l)
        return;

    if (!node)
        return;

    if (!glist_contains(l, node))
        return;

    /*if (node->prev)
        node->prev->next = node->next;
    if (node->next)
        node->next->prev = node->prev;
    if (l->head == node)
        l->head = node->next;
    if (l->tail == node)
        l->tail = node->prev;
    */

    if (node->prev)
        node->prev->next = node->next;
    else
        l->head = node->next;

    if (node->next)
        node->next->prev = node->prev;
    else
        l->tail = node->prev;

    --l->count;
    *node = (glist_node_t){0};
    free(node);
}

int glist_contains(glist_t *l, glist_node_t *n)
{
    if (!l)
        return -EINVAL;
    
    forlinked(tmp, l->head, tmp->next)
    {
        if (tmp == n)
            return 1;
    }

    return 0;
}