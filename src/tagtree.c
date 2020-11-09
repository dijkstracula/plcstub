/* tagtree.c
 * 
 * Manages the red-black tree of tags.
 * author: ntaylor
 */

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

/* Ensures mutual exclusion on the tag tree.  Does not ensure mutual
 * exclusion on individual tags in the tree, however! */
static pthread_rwlock_t tag_tree_mtx = PTHREAD_RWLOCK_INITIALIZER;

/* Tag storage and lookup. */

static int
tagcmp(struct tag_tree_node* lhs, struct tag_tree_node* rhs);

RB_HEAD(tag_tree_t, tag_tree_node)
tag_tree = RB_INITIALIZER(&tag_tree);
static size_t tree_size = 0;

RB_PROTOTYPE(tag_tree_t, tag_tree_node, rb_entry, tagcmp);
RB_GENERATE(tag_tree_t, tag_tree_node, rb_entry, tagcmp);



/* Allocates and initialises a fresh tag in the tag tree. */
struct tag_tree_node *
tag_tree_create_node()
{
    struct tag_tree_node* tag;
    int id;

    /* TODO: special case for the empty tree?. */
    tag = RB_MAX(tag_tree_t, &tag_tree);
    if (tag == NULL) {
        id = 2; /* id=1 = "@tags" */
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

    RW_WRLOCK(&tag_tree_mtx);

    tag->tag_id = id; 
    RB_INSERT(tag_tree_t, &tag_tree, tag);
    tree_size++;

    RW_UNLOCK(&tag_tree_mtx);

    return tag;
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
        return;
    }

    /* Upgrade to a writer lock and initialise. */
    RW_UNLOCK(&tag_tree_mtx);
    RW_WRLOCK(&tag_tree_mtx);

    /* Did somebody beat us to initing? If so, lucky us. */
    if (tag_tree_inited) {
        return;
    }

    pdebug(PLCTAG_DEBUG_DETAIL, "Initing");
    tag_tree_inited = true;
    RW_UNLOCK(&tag_tree_mtx);

    /* TODO: this should be done as part of a helper that
     * plc_tag_create can call.
     */
    for (int i = 0; i < NTAGS; ++i) {
        char* name, *data;
        size_t elem_count = 1;
        size_t elem_size = sizeof(uint32_t);

        struct tag_tree_node* tag = tag_tree_create_node();

        asprintf(&name, "DUMMY_AQUA_DATA_%d", i);
        if (name == NULL) {
            err(1, "asnprintf");
        }

        data = calloc(1, elem_count * elem_size);
        if (!data) {
            err(1, "calloc");
        }
        *(uint16_t*)(data) = i;
        
        MTX_LOCK(&tag->mtx);
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

    RW_RDLOCK(&tag_tree_mtx);

    struct tag_tree_node find;
    find.tag_id = tag_id;
    ret = RB_FIND(tag_tree_t, &tag_tree, &find);

    RW_UNLOCK(&tag_tree_mtx);

    return ret;
}


/* Compares two tag structures by ID. */
static int
tagcmp(struct tag_tree_node* lhs, struct tag_tree_node* rhs)
{
    return (lhs->tag_id < rhs->tag_id ? -1 : (lhs->tag_id > rhs->tag_id));
}
