/* plcstub.c
 *
 * Routines for the top-level interface to the PLC.
 * author: ntaylor
 */

#include <err.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "debug.h"
#include "plcstub.h"

static int
tagcmp(struct tag* lhs, struct tag* rhs);

/* Ensures mutual exclusion on all global plcstub data structures. */
static pthread_rwlock_t plcstub_mtx = PTHREAD_RWLOCK_INITIALIZER;

/* Tag storage and lookup. */

RB_HEAD(tag_tree_t, tag)
tag_tree = RB_INITIALIZER(&tag_tree);
RB_PROTOTYPE(tag_tree_t, tag, rb_entry, tagcmp);
RB_GENERATE(tag_tree_t, tag, rb_entry, tagcmp);

static bool plcstub_inited = false; /* Have we called plcstub_init() yet? */

/* invoked the first time the user of the library tries to do anything
 * with the the PLC.
 * 
 * Assumes that plcstub_mtx is NOT held.
 */
static void
plcstub_init()
{
    int ret;

    /* Check to see if we've inited.  If so, nothing to do. */
    if ((ret = pthread_rwlock_rdlock(&plcstub_mtx)) != 0) {
        err(1, "pthread_rwlock_rdlock");
    }

    if (plcstub_inited) {
        goto done;
    }

    if ((ret = pthread_rwlock_unlock(&plcstub_mtx)) != 0) {
        err(1, "pthread_rwlock_unlock");
    }

    /* Upgrade to a writer lock and initialise. */
    if ((ret = pthread_rwlock_wrlock(&plcstub_mtx)) != 0) {
        err(1, "pthread_rwlock_wrlock");
    }

    pdebug(PLCTAG_DEBUG_DETAIL, "Initing");

    for (int i = 0; i < NTAGS; ++i) {
        char* name;
        struct tag* tag;

        asprintf(&name, "DUMMY_AQUA_DATA_%d", i);
        if (name == NULL) {
            err(1, "asnprintf");
        }
        tag = malloc(sizeof(struct tag));
        if (tag == NULL) {
            err(1, "malloc");
        }
        tag->name = name;
        tag->tag_id = i;

        RB_INSERT(tag_tree_t, &tag_tree, tag);
    }

    /* Did somebody beat us to initing? If so, lucky us. */
    if (plcstub_inited) {
        goto done;
    }

done:
    if ((ret = pthread_rwlock_unlock(&plcstub_mtx)) != 0) {
        err(1, "pthread_rwlock_unlock");
    }
}

static int
tagcmp(struct tag* lhs, struct tag* rhs)
{
    return (lhs->tag_id < rhs->tag_id ? -1 : (lhs->tag_id > rhs->tag_id));
}

/************************ Public API ************************/

void
plc_tag_set_debug_level(int level)
{
    plcstub_init();
    set_debug_level(level);
}

int
plc_tag_get_debug_level()
{
    plcstub_init();
    return get_debug_level();
}

int
plc_tag_register_callback(int32_t tag_id, tag_callback_func cb)
{
    int ret;

    if ((ret = pthread_rwlock_wrlock(&plcstub_mtx)) != 0) {
        err(1, "pthread_rwlock_wrlock");
    }

    if ((ret = pthread_rwlock_unlock(&plcstub_mtx)) != 0) {
        err(1, "pthread_rwlock_unlock");
    }

    return 0;
}
