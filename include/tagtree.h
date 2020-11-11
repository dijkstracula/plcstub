#ifndef _TAGTREE_H_
#define _TAGTREE_H_

#include "plcstub.h"

#include <pthread.h>
#include <string.h>

/* The tag ID for the "@tag" metatag. */
#define METATAG_ID 1

struct tag_tree_node {
    RB_ENTRY(tag_tree_node)
    rb_entry;
    int tag_id;
    char* name; /* TODO: TAG_BASE_STRUCT doesn't contain a name: where does the name live? */
    tag_callback_func cb;
    pthread_mutex_t mtx;

    size_t elem_size;
    size_t elem_count;

    /* of length (elem_size * elem_count) 
     * TODO: can this buffer ever be resized?  If not, let's make it a char[0] and save
     * an allocation. */
    char* data;
};

struct tag_tree_node*
tag_tree_metanode_create();

struct tag_tree_node*
tag_tree_node_create();

void
tag_tree_node_destroy(struct tag_tree_node*);

struct tag_tree_node*
tag_tree_lookup(int32_t tag_id);

int
tag_tree_insert(struct tag_tree_node* node);

int
tag_tree_remove(int32_t tag_id);

#endif