#ifndef _TAGTREE_H_
#define _TAGTREE_H_

#include "plcstub.h"
#include "types.h"

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

    type_t type;

    /* of length (elem_size * elem_count) 
     * TODO: can this buffer ever be resized?  If not, let's make it a char[0] and save
     * an allocation. */
    char* data;
};

int
tag_tree_insert(const char* name, type_t type);

struct tag_tree_node*
tag_tree_lookup(int32_t tag_id);

int
tag_tree_remove(int32_t tag_id);

#endif