#ifndef G_LIST_H
#define G_LIST_H    1

typedef struct __glist_node
{
    void *ptr;
    struct __glist_node *prev, *next;
} glist_node_t;

typedef struct __glist
{
    int count;
    glist_node_t *head, *tail;
} glist_t;

#define GLIST_NEW() &(glist_t){0};

int glist_init(glist_t *, glist_t **);
void glist_free(glist_t *);

int glist_add(glist_t *, void *);
void *glist_get(glist_t *);
int glist_del(glist_t *, void *);

void glist_remove(glist_t *, glist_node_t *);
glist_node_t *glist_lookup(glist_t *, void *);
int glist_contains(glist_t *, glist_node_t *);

#endif //G_LIST_H