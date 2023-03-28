#pragma once

#include <lib/stdint.h>
#include <lib/stddef.h>
#include <lime/assert.h>
#include <locks/spinlock.h>

#ifndef foreach
#define foreach(elem, list) \
    for (typeof(*list) *tmp = list, elem = *tmp; elem; elem = *++tmp)
#endif // foreach

#ifndef forlinked
#define forlinked(elem, list, iter) \
    for (typeof(list) elem = list; elem; elem = iter)
#endif // forlinked

typedef uintptr_t btree_key_t;

struct btree;

typedef struct btree_node
{
    btree_key_t          key;
    void                *data;
    struct btree_node   *left;
    struct btree_node   *parent;
    struct btree_node   *right;
    struct btree        *btree;
} btree_node_t;

typedef struct btree
{
    btree_node_t *root;
    size_t      nr_nodes;
    spinlock_t *lock;
} btree_t;

#define btree_assert(btree) assert(btree, "No Btree")
#define btree_assert_locked(btree) {btree_assert(btree); spin_assert_lock(btree->lock);}

#define btree_nr_nodes(btree) ({btree_assert_locked(btree); (btree->nr_nodes); })
#define btree_isempty(btree) ({ btree_assert_locked(btree); (btree->root == NULL);})

#define btree_node_isleft(node)     ({ node->parent ? node->parent->left == node ? 1 : 0 : 0;})
#define btree_node_isright(node)    ({ node->parent ? node->parent->right == node ? 1 : 0 : 0;})

/**
 * @brief btree_alloc_node
*/
btree_node_t *btree_alloc_node(void);

/**
 * @bbtree_free_noderief 
*/
void btree_free_node(btree_node_t *node);

/**
 * @brief btree_least
*/
btree_node_t *btree_least_node(btree_t *btree);

/**
 * @brief btree_least
*/
static inline void *btree_least(btree_t *btree)
{
    btree_node_t *node = NULL;

    if (btree == NULL)
        return NULL;
    node = btree_least_node(btree);
    if (node == NULL)
        return NULL;
    return node->data;
}

/**
 * @brief btree_largest
*/
btree_node_t *btree_largest_node(btree_t *btree);

/**
 * @brief btree_largest
*/
static inline void *btree_largest(btree_t *btree)
{
    btree_node_t *node = NULL;

    if (btree == NULL)
        return NULL;
    node = btree_largest_node(btree);
    if (node == NULL)
        return NULL;
    return node->data;
}

/**
 * @bbtree_deleterief 
*/
void btree_delete(btree_t *btree, btree_key_t key);

/**
 * @brbtree_searchief 
*/
int btree_search(btree_t *btree, btree_key_t key, void **pdata);

/**
 * @brief btree_lookup
*/
btree_node_t *btree_lookup(btree_t *btree, btree_key_t key);

/**
 * @btree_insertbrief 
*/
int btree_insert(btree_t *btree, btree_key_t key, void *data);
