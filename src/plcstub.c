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

#include "libplctag.h"

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

    /* Upgrade to a writer lock and initialise. */
    if ((ret = pthread_rwlock_unlock(&plcstub_mtx)) != 0) {
        err(1, "pthread_rwlock_unlock");
    }
    if ((ret = pthread_rwlock_wrlock(&plcstub_mtx)) != 0) {
        err(1, "pthread_rwlock_wrlock");
    }

    /* Did somebody beat us to initing? If so, lucky us. */
    if (plcstub_inited) {
        goto done;
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

    plcstub_inited = true;

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

int
plc_tag_get_debug_level()
{
    plcstub_init();
    return get_debug_level();
}

void
plc_tag_set_debug_level(int level)
{
    plcstub_init();
    set_debug_level(level);
}

int
plc_tag_set_int32(int32_t tag, int offset, int value)
{
    int ret;

    struct tag find, *t;

    /* TODO: how is offset meant to be used within a tag? */

    plcstub_init();

    if (pthread_rwlock_wrlock(&plcstub_mtx)) {
        err(1, "pthread_rwlock_wrlock");
    }

    find.tag_id = tag;
    t = RB_FIND(tag_tree_t, &tag_tree, &find);
    if (!t) {
        pdebug(PLCTAG_DEBUG_WARN, "Unknown tag %d", tag);
        ret = PLCTAG_ERR_NOT_FOUND;
        goto done;
    }

    if (t->cb) {
        pdebug(PLCTAG_DEBUG_SPEW,
            "Calling cb for %d with PLCTAG_WRITE_EVENT_STARTED", tag);
        t->cb(tag, PLCTAG_EVENT_WRITE_STARTED, PLCTAG_STATUS_OK);
    }

    t->val = value;

    if (t->cb) {
        pdebug(PLCTAG_DEBUG_SPEW,
            "Calling cb for %d with PLCTAG_WRITE_EVENT_COMPLETED", tag);
        t->cb(tag, PLCTAG_EVENT_WRITE_COMPLETED, PLCTAG_STATUS_OK);
    }

done:
    if (pthread_rwlock_unlock(&plcstub_mtx)) {
        err(1, "pthread_rwlock_unlock");
    }
    return ret;
};

int
plc_tag_register_callback(int32_t tag_id, tag_callback_func cb)
{
    int ret = PLCTAG_STATUS_OK;
    struct tag find, *t;

    plcstub_init();

    if (pthread_rwlock_wrlock(&plcstub_mtx)) {
        err(1, "pthread_rwlock_wrlock");
    }

    find.tag_id = tag_id;
    t = RB_FIND(tag_tree_t, &tag_tree, &find);
    if (!t) {
        pdebug(PLCTAG_DEBUG_WARN, "Unknown tag %d", tag_id);
        ret = PLCTAG_ERR_NOT_FOUND;
        goto done;
    }
    t->cb = cb;

done:
    if (pthread_rwlock_unlock(&plcstub_mtx)) {
        err(1, "pthread_rwlock_unlock");
    }

    return ret;
}
