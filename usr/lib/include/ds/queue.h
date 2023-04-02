#pragma once

typedef struct queue_node
{
    void *data;
    struct *prev, *next;
} queue_node_t;

typedef struct queue
{
    int count;
    queue_node_t *head, *tail;
} queue_t;

