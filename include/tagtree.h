#ifndef _TAGTREE_H_
#define _TAGTREE_H_

#include "plcstub.h"

#include <pthread.h>
#include <string.h>

struct tag_tree_node {
    RB_ENTRY(tag_tree_node)
    rb_entry;
    int tag_id;
    char* name; /* TODO: TAG_BASE_STRUCT doesn't contain a name: where does the name live? */
    tag_callback_func cb;
    pthread_mutex_t mtx;

    size_t elem_size;
    size_t elem_count;

    /* of length (elem_size * elem_count) */
    char* data;
};

struct tag_tree_node*
tag_tree_create_node();

struct tag_tree_node*
tag_tree_lookup(int32_t tag_id);

int
tag_tree_insert(struct tag_tree_node* node);

int
tag_tree_remove(int32_t tag_id);

#endif