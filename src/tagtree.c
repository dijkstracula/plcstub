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

RB_HEAD(tag_tree_t, tag_tree_node)
tag_tree = RB_INITIALIZER(&tag_tree);
static size_t tree_size = 0;

static int
tagcmp(struct tag_tree_node* lhs, struct tag_tree_node* rhs);

RB_PROTOTYPE(tag_tree_t, tag_tree_node, rb_entry, tagcmp);
RB_GENERATE(tag_tree_t, tag_tree_node, rb_entry, tagcmp);

static void
tag_tree_init();
static struct tag_tree_node*
tag_tree_node_create(const char*, type_t);
static void
tag_tree_node_destroy();

/* Compares two tag structures by ID. */
static int
tagcmp(struct tag_tree_node* lhs, struct tag_tree_node* rhs)
{
    return (lhs->tag_id < rhs->tag_id ? -1 : (lhs->tag_id > rhs->tag_id));
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

/* TODO: these should likely be functions. */
#define DEFINE_SCALAR(name, type, val)                           \
    do {                                                         \
        struct tag_tree_node* tag;                               \
        tag = tag_tree_node_create(name, type_new_simple(type)); \
        *tag->data = val;                                        \
        MTX_UNLOCK(&tag->mtx);                                   \
    } while (0);
#define DEFINE_ARRAY(name, element_type, size, val) err(1, "DEFINE_ARRAY: not implemented yet");
#include "tags.inc"
#undef DEFINE_SCALAR

    RW_UNLOCK(&tag_tree_mtx);
}

/* Allocates and initialises a fresh tag  In order to ensure no tags
 * are duplicated, tag_tree_mtx must already be held by the caller. 
 * The node that is returned is inserted into the tree BUT has its
 * mutex held.  The caller will populate its fields and unlock it
 * when it is ready to be visible. */
static struct tag_tree_node*
tag_tree_node_create(const char* name, type_t type)
{
    struct tag_tree_node *tag, *metatag, find;
    size_t sz;
    int id;

    sz = type_size_bytes(type);
    /* Reserve at least a word of data.  This simplifies implementing the DEFINE_SCALAR macro:
     * where we don't have to worry about integer promotion. */
    if (sz < sizeof(uintptr_t)) {
        sz = sizeof(uintptr_t);
    }

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

    tag->name = strdup(name);
    if (!tag->name) {
        errx(1, "strdup");
    }

    tag->type = type_dup(type);
    if (!tag->type) {
        err(1, "type_dup");
    }

    tag->data = malloc(sz);
    if (tag->data == NULL) {
        err(1, "malloc");
    }
    memset(tag->data, sz, 0x42);

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

    MTX_LOCK(&tag->mtx);
    return tag;
}

static void
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

/* Creates the special "@tags" metanode, the tag containing an array
 * of all tags.  The tag tree node lock is assumed to be held by the caller!
 * 
 * Unlike tag_tree_create(), because no population of fields is necessary,
 * the metanode is _not_ locked before it is returned to the caller.  (Would
 * it be better if we did that, for API consistency?)
 */
static struct tag_tree_node*
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
    tag->cb = NULL;

    /* XXX: because the entries are variable in length, this can't really be represented
     * in plcstub's type system.  So, make it an array of bytes.
     */
    tag->type = type_new_array(total_data_size, type_new_simple(TAG_SINT));

    tag->data = p = malloc(total_data_size);
    if (!p) {
        err(1, "malloc");
    }

    pdebug(PLCTAG_DEBUG_DETAIL, "Creating @tags metatag (node ID %d) (%d bytes)", METATAG_ID, type_size_bytes(tag->type));

    /* XXX: Currently these results should not be relied upon too much :-( */
    RB_FOREACH(tag, tag_tree_t, &tag_tree)
    {
        struct metatag_t* mt = (struct metatag_t*)(p);
        mt->id = tag->tag_id;
        /* TODO: need a way of converting a type_t to this representation */
        mt->type = (1 << 13); /* TODO: type needs more than the dimensions mask */

        mt->elem_size = type_size_bytes(tag->type); /* TODO: this is wrong for arrays */

        if (type_is_scalar(tag->type) || type_to_enum(tag->type) == TAG_STRUCT) {
            mt->array_dims[0] = 0;
        } else {
            struct tag_array* a = (struct tag_array*)(tag->type);
            mt->array_dims[0] = a->len;
        }
        mt->array_dims[1] = mt->array_dims[2] = 0;
        mt->length = strlen(tag->name);
        memcpy(mt->data, tag->name, mt->length);

        p = p + sizeof(struct metatag_t) + mt->length;
    }

    pdebug(PLCTAG_DEBUG_SPEW, "Wrote %d of %d bytes as metatag data", (p - ret->data), total_data_size);

    RB_INSERT(tag_tree_t, &tag_tree, ret);

    return ret;
}

/* 
 * Allocates and inserts a new tag node into the tag tree with the given name
 * and sizing metadata.  If the magic name "@tags" is given, the tag metanode
 * is revalidated instead.
 */
int
tag_tree_insert(const char* name, type_t type)
{
    int ret;
    struct tag_tree_node* tag;

    tag_tree_init();

    RW_WRLOCK(&tag_tree_mtx);
    if (strcmp(name, "@tags") == 0) {
        tag = tag_tree_metanode_create();
        if (tag == NULL) {
            err(1, "tag_tree_metanode_create");
        }
        ret = tag->tag_id;
    } else {
        tag = tag_tree_node_create(name, type);
        if (tag == NULL) {
            err(1, "tag_tree_node_create");
        }
        MTX_UNLOCK(&tag->mtx);
        ret = tag->tag_id;
    }
    RW_UNLOCK(&tag_tree_mtx);
    return ret;
}

int
tag_tree_remove(int32_t id)
{
    struct tag_tree_node* tag;

    if (id == METATAG_ID) {
        // Unclear why we would want to remove this, but
        // silently accept it.
        return PLCTAG_STATUS_OK;
    }

    tag_tree_init();

    RW_WRLOCK(&tag_tree_mtx);

    struct tag_tree_node find;
    find.tag_id = id;
    tag = RB_FIND(tag_tree_t, &tag_tree, &find);
    if (!tag) {
        pdebug(PLCTAG_DEBUG_WARN, "Lookup for tag %d failed", id);
        RW_UNLOCK(&tag_tree_mtx);
        return PLCTAG_ERR_NOT_FOUND;
    }

    MTX_LOCK(&tag->mtx);

    /* TODO: special case for the empty tree?. */
    RB_REMOVE(tag_tree_t, &tag_tree, tag);
    tree_size--;

    RW_UNLOCK(&tag_tree_mtx);
    MTX_UNLOCK(&tag->mtx);

    tag_tree_node_destroy(tag);

    pdebug(PLCTAG_DEBUG_DETAIL, "Removed tag %d", id);

    return PLCTAG_STATUS_OK;
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
