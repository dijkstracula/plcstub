/* plcstub.c
 *
 * Routines for the top-level interface to the PLC.
 * author: ntaylor
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>

#include "debug.h"
#include "plcstub.h"

static int tagcmp(struct tag *lhs, struct tag *rhs);

/* Ensures mutual exclusion on all global plcstub data structures. */
static pthread_rwlock_t plcstub_mtx = PTHREAD_RWLOCK_INITIALIZER;

/* Tag storage and lookup. */

RB_HEAD(tag_tree_t, tag) tag_tree = RB_INITIALIZER(&tag_tree);
RB_PROTOTYPE(tag_tree_t, tag, rb_entry, tagcmp);
RB_GENERATE(tag_tree_t, tag, rb_entry, tagcmp);

static bool plcstub_inited = false; /* Have we called plcstub_init() yet? */

/* invoked the first time the user of the library tries to do anything
 * with the the PLC.
 * 
 * Assumes that plcstub_mtx is already held.
 */
static void plcstub_init() {

}

static int
tagcmp(struct tag *lhs, struct tag *rhs) {
    return (lhs->tag_id < rhs->tag_id ? -1 : (lhs->tag_id > rhs->tag_id));
}

/* Public API */

void plc_tag_set_debug_level(int level) {
    set_debug_level(level); 
}

int plc_tag_register_callback(int32_t tag_id, tag_callback_func cb) {
    int ret;

    if ((ret = pthread_rwlock_wrlock(&plcstub_mtx)) != 0) {
        err(1, "pthread_rwlock_wrlock");
    }

    if ((ret = pthread_rwlock_unlock(&plcstub_mtx)) != 0) {
        err(1, "pthread_rwlock_unlock");
    }
}

