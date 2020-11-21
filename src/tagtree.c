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
#include "libplctag.h"
#include "lock_utils.h"
#include "plcstub.h"
#include "tagtree.h"

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

/* Creates the special "@tags" metanode, the tag containing an array
 * of all tags.  The tag tree node lock is assumed to be held by the caller!
 * 
 * Unlike tag_tree_create(), because no population of fields is necessary,
 * the metanode is _not_ locked before it is returned to the caller.  (Would
 * it be better if we did that, for API consistency?)
 */
struct tag_tree_node*
tag_tree_metanode_create()
{
    size_t total_data_size = 0;
    struct tag_tree_node *tag, *ret;
    char* p;

    RB_FOREACH(tag, tag_tree_t, &tag_tree)
    {
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

    RB_FOREACH(tag, tag_tree_t, &tag_tree)
    {
        struct metatag_t* mt = (struct metatag_t*)(p);
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

    return ret;
}

/* Allocates and initialises a fresh tag  In order to ensure no tags
 * are duplicated, tag_tree_mtx must already be held by the caller. 
 * The node that is returned is inserted into the tree BUT has its
 * mutex held.  The caller will populate its fields and unlock it
 * when it is ready to be visible. */
struct tag_tree_node*
tag_tree_node_create(const char* name, size_t elem_size, size_t elem_count)
{
    struct tag_tree_node *tag, *metatag, find;
    int id;

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
    MTX_LOCK(&tag->mtx);

    tag->name = strdup(name);
    if (!tag->name) {
        errx(1, "strdup");
    }

    tag->elem_count = elem_count;
    tag->elem_size = elem_size;

    tag->data = calloc(tag->elem_count, tag->elem_size);
    if (tag->data == NULL) {
        err(1, "calloc");
    }

    tag->tag_id = id;
    RB_INSERT(tag_tree_t, &tag_tree, tag);
    tree_size++;

    /* Invalidate the metatag, if one exists. */
    find.tag_id = METATAG_ID;
    metatag = RB_FIND(tag_tree_t, &tag_tree, &find);
    if (metatag != NULL) {
        RB_REMOVE(tag_tree_t, &tag_tree, metatag);
        tree_size--;
        tag_tree_node_destroy(metatag);
    }

    pdebug(PLCTAG_DEBUG_DETAIL, "Created new tag %d (%s)", id, name);

    return tag;
}

void
tag_tree_node_destroy(struct tag_tree_node* tag)
{
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
tag_tree_remove(int32_t id)
{
    struct tag_tree_node* tag;

    tag_tree_init();

    tag = tag_tree_lookup(id);
    if (!tag) {
        pdebug(PLCTAG_DEBUG_WARN, "Lookup for tag %d failed", id);
        RW_UNLOCK(&tag_tree_mtx);
        return PLCTAG_ERR_NOT_FOUND;
    }
    MTX_LOCK(&tag->mtx);

    /* FIXME: This is racy: two threads racing on removing the same
     * ID could yield Weirdness given that IDs get reassigned. */
    RW_WRLOCK(&tag_tree_mtx);

    /* TODO: special case for the empty tree?. */
    RB_REMOVE(tag_tree_t, &tag_tree, tag);
    tree_size--;

    RW_UNLOCK(&tag_tree_mtx);

    MTX_UNLOCK(&tag->mtx);

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

    pdebug(PLCTAG_DEBUG_DETAIL, "Initing");

    /* TODO: this should be done as part of a helper that
     * consumes dummy tags from an input file or something.
     */
    for (int i = 0; i < NTAGS; ++i) {
        char name[1024];
        size_t elem_count = 1;
        size_t elem_size = sizeof(uint32_t);

        snprintf(name, sizeof(name), "DUMMY_AQUA_DATA_%d", i);

        struct tag_tree_node* tag = tag_tree_node_create(name, elem_size, elem_count);
        *(uint16_t*)(tag->data) = i;
        MTX_UNLOCK(&tag->mtx);
    }
    RW_UNLOCK(&tag_tree_mtx);
}

/* Looks up a tag by ID; returns NULL if no such tag exists. 
 *
 * This function does NOT eagerly lock the returned tag; it
 * falls to the caller to do so!
 */
struct tag_tree_node*
tag_tree_lookup(int32_t tag_id)
{
    struct tag_tree_node* ret;

    tag_tree_init();

    pdebug(PLCTAG_DEBUG_DETAIL, "Looking up tag id %d", tag_id);

    if (tag_id == METATAG_ID) {
        /* we may have to refresh the metanode tag, so we need
         * exclusive access to the rwlock for the metatag. */
        RW_WRLOCK(&tag_tree_mtx);
    } else {
        RW_RDLOCK(&tag_tree_mtx);
    }

    struct tag_tree_node find;
    find.tag_id = tag_id;
    ret = RB_FIND(tag_tree_t, &tag_tree, &find);

    if (ret == NULL && tag_id == METATAG_ID) {
        ret = tag_tree_metanode_create();
    }

    RW_UNLOCK(&tag_tree_mtx);
    return ret;
}

/* Compares two tag structures by ID. */
static int
tagcmp(struct tag_tree_node* lhs, struct tag_tree_node* rhs)
{
    return (lhs->tag_id < rhs->tag_id ? -1 : (lhs->tag_id > rhs->tag_id));
}
