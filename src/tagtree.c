/* tagtree.c
 * 
 * Manages the red-black tree of tags.
 * author: ntaylor
 */

#ifndef __uintptr_t
  #define __uintptr_t uintptr_t
#endif

#include <err.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "plcstub.h"
#include "libplctag.h"
#include "tagtree.h"
#include "lock_utils.h"

/* 
 * Ensures mutual exclusion on the tag tree and metatag. Does not ensure mutual
 * exclusion on individual tags in the tree, however! *
 */
static pthread_rwlock_t tag_tree_mtx = PTHREAD_RWLOCK_INITIALIZER;

/* Tag storage and lookup. */

static int
tagcmp(struct tag_tree_node* lhs, struct tag_tree_node* rhs);

RB_HEAD(tag_tree_t, tag_tree_node)
tag_tree = RB_INITIALIZER(&tag_tree);
static size_t tree_size = 0;

RB_PROTOTYPE(tag_tree_t, tag_tree_node, rb_entry, tagcmp);
RB_GENERATE(tag_tree_t, tag_tree_node, rb_entry, tagcmp);

static void
tag_tree_init();

struct tag_tree_node *
tag_tree_metanode_create() {
    size_t total_data_size = 0;
    struct tag_tree_node *tag, *ret;
    char *p;


    RW_WRLOCK(&tag_tree_mtx);
 
    RB_FOREACH(tag, tag_tree_t, &tag_tree) {
        total_data_size += sizeof(struct metatag_t) + strlen(tag->name);
    }
    
    
    struct tag_tree_node find;
    find.tag_id = METATAG_ID;
    tag = RB_FIND(tag_tree_t, &tag_tree, &find);
    if (tag != NULL) {
        RB_REMOVE(tag_tree_t, &tag_tree, tag);
        tree_size--;
        tag_tree_node_destroy(tag);
    }

    ret = tag = malloc(sizeof(struct tag_tree_node));
    if (tag == NULL) {
        err(1, "malloc");
    }
    if (pthread_mutex_init(&tag->mtx, NULL)) {
        err(1, "pthread_mutex_init");
    }
    
    tag->name = strdup("@tags");
    tag->tag_id = METATAG_ID;
    tag->elem_count = 1;
    tag->elem_size = total_data_size;
    tag->data = p = malloc(tag->elem_size);
    tag->cb = NULL;
    
    pdebug(PLCTAG_DEBUG_DETAIL, "Creating @tags metatag (node ID %d) (%d bytes)", METATAG_ID, tag->elem_size);

    RB_FOREACH(tag, tag_tree_t, &tag_tree) {
        struct metatag_t *mt = (struct metatag_t *)(p);
        mt->id = tag->tag_id;
        mt->type = (1 << 13); /* TODO: type needs more than the dimensions mask */
        mt->elem_size = tag->elem_size;
        mt->array_dims[0] = tag->elem_count;
        mt->array_dims[1] = mt->array_dims[2] = 0; 
        mt->length = strlen(tag->name);
        memcpy(mt->data, tag->name, mt->length);

        p = p + sizeof(struct metatag_t) + mt->length;
    }

    pdebug(PLCTAG_DEBUG_SPEW, "Wrote %d of %d bytes as metatag data", (p - ret->data), total_data_size);

    RB_INSERT(tag_tree_t, &tag_tree, ret);

    RW_UNLOCK(&tag_tree_mtx);

    return ret;
}

/* Allocates and initialises a fresh tag in the tag tree. */
struct tag_tree_node *
tag_tree_node_create()
{
    struct tag_tree_node* tag, *metatag, find;
    int id;

    tag_tree_init();
    
    RW_WRLOCK(&tag_tree_mtx);

    /* TODO: special case for the empty tree?. */
    tag = RB_MAX(tag_tree_t, &tag_tree);
    if (tag == NULL) {
        id = METATAG_ID + 1;
    } else {
        id = tag->tag_id + 1;
    }

    tag = malloc(sizeof(struct tag_tree_node));
    if (tag == NULL) {
        err(1, "malloc");
    }
    memset(tag, 0, sizeof(struct tag_tree_node));

    if (pthread_mutex_init(&tag->mtx, NULL)) {
        err(1, "pthread_mutex_init");
    }

    /* TODO: should we take the lock before inserting
     * and let the caller unlock it so that the first 
     * reader is guaranteed to see the complete object?
     */

    tag->tag_id = id; 
    RB_INSERT(tag_tree_t, &tag_tree, tag);
    tree_size++;


    find.tag_id = METATAG_ID;
    metatag = RB_FIND(tag_tree_t, &tag_tree, &find);
    if (metatag != NULL) {
        RB_REMOVE(tag_tree_t, &tag_tree, metatag);
        tree_size--;
        tag_tree_node_destroy(metatag);
    }

    RW_UNLOCK(&tag_tree_mtx);
    

    pdebug(PLCTAG_DEBUG_DETAIL, "Created new tag %d", id);

    return tag;
}

void
tag_tree_node_destroy(struct tag_tree_node *tag) {
    if (!tag) {
        return;
    }
    
    pdebug(PLCTAG_DEBUG_DETAIL, "Destroying node %d", tag->tag_id);

    MTX_LOCK(&tag->mtx);

    pthread_mutex_t mtx = tag->mtx;
    free(tag->data);
    free(tag->name);
    free(tag);

    MTX_UNLOCK(&mtx);

    pthread_mutex_destroy(&mtx);
}

int
tag_tree_remove(int32_t id) {
    struct tag_tree_node* tag;

    tag_tree_init();
    
    tag = tag_tree_lookup(id);
    if (!tag) {
        pdebug(PLCTAG_DEBUG_WARN, "Lookup for tag %d failed", id);
        RW_UNLOCK(&tag_tree_mtx);
        return PLCTAG_ERR_NOT_FOUND;
    }

    /* FIXME: This is racy: two threads racing on removing the same
     * ID could yield Weirdness given that IDs get reassigned. */
    RW_WRLOCK(&tag_tree_mtx);

    /* TODO: special case for the empty tree?. */
    RB_REMOVE(tag_tree_t, &tag_tree, tag);
    tree_size--;
    
    RW_UNLOCK(&tag_tree_mtx);

    tag_tree_node_destroy(tag);

    pdebug(PLCTAG_DEBUG_DETAIL, "Removed tag %d", id);

    return PLCTAG_STATUS_OK;
}

/* invoked the first time the user of the library tries to do anything
 * with the the PLC.
 * 
 * Assumes that tag_tree_mtx is NOT held.
 */
static void
tag_tree_init()
{
    static bool tag_tree_inited = false; /* Have we called tag_tree_init() yet? */

    /* Check to see if we've inited.  If so, nothing to do. */
    RW_RDLOCK(&tag_tree_mtx);

    if (tag_tree_inited) {
        RW_UNLOCK(&tag_tree_mtx);
        return;
    }

    /* Upgrade to a writer lock and initialise. */
    RW_UNLOCK(&tag_tree_mtx);
    RW_WRLOCK(&tag_tree_mtx);

    /* Did somebody beat us to initing? If so, lucky us. */
    if (tag_tree_inited) {
        RW_UNLOCK(&tag_tree_mtx);
        return;
    }
    tag_tree_inited = true;
    RW_UNLOCK(&tag_tree_mtx);

    pdebug(PLCTAG_DEBUG_DETAIL, "Initing");


    /* TODO: this should be done as part of a helper that
     * consumes dummy tags from an input file or something.
     */
    for (int i = 0; i < NTAGS; ++i) {
        char* name, *data;
        size_t elem_count = 1;
        size_t elem_size = sizeof(uint32_t);

        struct tag_tree_node* tag = tag_tree_node_create();
        MTX_LOCK(&tag->mtx);

        asprintf(&name, "DUMMY_AQUA_DATA_%d", i);
        if (name == NULL) {
            err(1, "asnprintf");
        }

        data = calloc(elem_count, elem_size);
        if (!data) {
            err(1, "calloc");
        }
        *(uint16_t*)(data) = i;
        
        tag->name = name;
        tag->data = data;
        tag->elem_count = elem_count;
        tag->elem_size = elem_size;
        MTX_UNLOCK(&tag->mtx);
    }
}

/* Looks up a tag by ID; returns NULL if no such tag exists. 
 *
 * This function does NOT eagerly lock the returned tag; it
 * falls to the caller to do so!
 */
struct tag_tree_node*
tag_tree_lookup(int32_t tag_id)
{
    struct tag_tree_node *ret;

    tag_tree_init();

    pdebug(PLCTAG_DEBUG_DETAIL, "Looking up tag id %d", tag_id);


    RW_RDLOCK(&tag_tree_mtx);

    struct tag_tree_node find;
    find.tag_id = tag_id;
    ret = RB_FIND(tag_tree_t, &tag_tree, &find);

    RW_UNLOCK(&tag_tree_mtx);

    if (ret == NULL && tag_id == METATAG_ID) {
        return tag_tree_metanode_create();
    }

    return ret;
}


/* Compares two tag structures by ID. */
static int
tagcmp(struct tag_tree_node* lhs, struct tag_tree_node* rhs)
{
    return (lhs->tag_id < rhs->tag_id ? -1 : (lhs->tag_id > rhs->tag_id));
}
