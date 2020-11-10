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
#include <string.h>

#include "debug.h"
#include "plcstub.h"
#include "libplctag.h"
#include "lock_utils.h"
#include "tagtree.h"

typedef void(getter_fn)(char *buf, int offset, void *val);
typedef void(setter_fn)(char *buf, int offset, void *val);

/* Accessor / mutator macros */

#define GETTER(name, type)                                                  \
static void                                                                 \
plcstub_##name##_getter_cb(char *buf, int offset, void *val) {              \
    pdebug(PLCTAG_DEBUG_SPEW, "reading at offset %d", offset);              \
    type *p = (type *)(buf + offset);                                       \
    *(type *)(val) = *p;                                                    \
}                                                                           \
type                                                                        \
plc_tag_get_##name (int32_t tag, int offset) {                              \
    type val;                                                               \
    plcstub_get_impl(tag, offset, &val, plcstub_##name##_getter_cb);        \
    return val;                                                             \
} 

#define SETTER(name, type)                                                  \
static void                                                                 \
plcstub_##name##_setter_cb(char *buf, int offset, void *val) {              \
    pdebug(PLCTAG_DEBUG_SPEW, "writing at offset %d", offset);              \
    type *p = (type *)(buf + offset);                                       \
    *p = *(type *)(val);                                                    \
}                                                                           \
int                                                                         \
plc_tag_set_##name (int32_t tag, int offset, type val) {                    \
    return plcstub_set_impl(tag, offset, &val, plcstub_##name##_setter_cb); \
}

#define TYPEMAP        \
/* X(name, type) */    \
X(bit, int)            \
X(uint64, uint64_t)    \
X(int64, int64_t)      \
X(uint32, uint32_t)    \
X(int32, int32_t)      \
X(uint16, uint16_t)    \
X(int16, int16_t)      \
X(uint8, uint8_t)      \
X(int8, int8_t)        \
X(float64, double)     \
X(float32, float)

static int
plcstub_get_impl(int32_t tag, int offset, void* buf, getter_fn fn)
{
    struct tag_tree_node* t;

    t = tag_tree_lookup(tag);
    if (!t) {
        pdebug(PLCTAG_DEBUG_WARN, "Unknown tag %d", tag);
        return PLCTAG_ERR_NOT_FOUND;
    }

    /* TODO: I'm not thrilled about holding the lock through the course of all
     * these callbacks, especially until we know the overhead of doing golang<->native
     * interop.  Maybe it's better to make a defensive copy where possible? */
    MTX_LOCK(&t->mtx);

    if (t->cb) {
        pdebug(PLCTAG_DEBUG_SPEW,
            "Calling cb for %d with PLCTAG_READ_EVENT_STARTED", tag);
        t->cb(tag, PLCTAG_EVENT_READ_STARTED, PLCTAG_STATUS_OK);
    }

    if (offset >= t->elem_count * t->elem_size) {
        pdebug(PLCTAG_DEBUG_WARN, 
            "Offset %d out of bounds of [0..%d)", offset, t->elem_count * t->elem_size);
        if (t->cb) {
            pdebug(PLCTAG_DEBUG_SPEW,
                "Calling cb for %d with PLCTAG_EVENT_ABORTED", tag);
            t->cb(tag, PLCTAG_EVENT_ABORTED, PLCTAG_ERR_BAD_PARAM);
        }
        return PLCTAG_ERR_BAD_PARAM;
    }

    fn(t->data, offset, buf);

    if (t->cb) {
        pdebug(PLCTAG_DEBUG_SPEW,
            "Calling cb for %d with PLCTAG_READ_EVENT_COMPLETED", tag);
        t->cb(tag, PLCTAG_EVENT_READ_COMPLETED, PLCTAG_STATUS_OK);
    }

    MTX_UNLOCK(&t->mtx);

    return PLCTAG_STATUS_OK;
}

static int
plcstub_set_impl(int32_t tag, int offset, void *value, setter_fn fn)
{
    struct tag_tree_node* t;

    t = tag_tree_lookup(tag);
    if (!t) {
        pdebug(PLCTAG_DEBUG_WARN, "Unknown tag %d", tag);
        return PLCTAG_ERR_NOT_FOUND;
    }

    /* TODO: I'm not thrilled about holding the lock through the course of all
     * these callbacks, especially until we know the overhead of doing golang<->native
     * interop.  Maybe it's better to make a defensive copy where possible? */
    MTX_LOCK(&t->mtx);

    if (t->cb) {
        pdebug(PLCTAG_DEBUG_SPEW,
            "Calling cb for %d with PLCTAG_WRITE_EVENT_STARTED", tag);
        t->cb(tag, PLCTAG_EVENT_WRITE_STARTED, PLCTAG_STATUS_OK);
    }

    if (offset >= t->elem_count * t->elem_size) {
        pdebug(PLCTAG_DEBUG_WARN, 
            "Offset %d out of bounds of [0..%d)", offset, t->elem_count * t->elem_size);
        if (t->cb) {
            pdebug(PLCTAG_DEBUG_SPEW,
                "Calling cb for %d with PLCTAG_EVENT_ABORTED", tag);
            t->cb(tag, PLCTAG_EVENT_ABORTED, PLCTAG_ERR_BAD_PARAM);
        }
        return PLCTAG_ERR_BAD_PARAM;
    }

    fn(t->data, offset, &value);

    if (t->cb) {
        pdebug(PLCTAG_DEBUG_SPEW,
            "Calling cb for %d with PLCTAG_WRITE_EVENT_COMPLETED", tag);
        t->cb(tag, PLCTAG_EVENT_WRITE_COMPLETED, PLCTAG_STATUS_OK);
    }

    MTX_UNLOCK(&t->mtx);

    return PLCTAG_STATUS_OK;
}

/************************ Public API ************************/


int
plc_tag_check_lib_version(int req_major, int req_minor, int req_patch) {
    (void)(req_major);
    (void)(req_minor);
    (void)(req_patch);
    return true;
}

int
plc_tag_get_debug_level()
{
    return debug_get_level();
}

int
plc_tag_create(const char* attrib, int timeout)
{
    int ret = PLCTAG_STATUS_OK;
    char *kv_ctx, *kv;
    char* kv_sep = "&";
    struct tag_tree_node* tag;

    /* There are three attributes that we are interested in at the moment:
     * 1) name: the name of the tag
     * 2) elem_size: the width of each element in the tag
     * 3) elem_count: how many elements. (TODO: how does this work with multi-dim arrays?)
     */
    char* name = NULL;

    /* TODO: It appears that we need not specify elem_size and elem_count.  What should
     * the expected "default" value be? 
     */
    size_t elem_size = 2;
    size_t elem_count = 1;

    char* str = strdup(attrib);

    for (kv = strtok_r(str, kv_sep, &kv_ctx);
         kv != NULL;
         kv = strtok_r(NULL, kv_sep, &kv_ctx)) {
        char *key, *val;
        pdebug(PLCTAG_DEBUG_SPEW, "Current kv-pair: %s", kv);

        key = kv;
        val = strchr(key, '=');
        if (val == NULL && (strcmp("protocol", kv) != 0)) {
            /* At the moment, the only attribute we've seen that isn't a key-value pair
             * is "protocol".  If we encounter others, we can either check for them or
             * just ignore them altogether, depending on our confidence of things. */
            pdebug(PLCTAG_DEBUG_WARN, "Missing '=' in non-'protocol' attribute %s", kv);
            ret = PLCTAG_ERR_BAD_PARAM;
            goto done;
        }
        *val = '\0';
        val++;
        pdebug(PLCTAG_DEBUG_SPEW, "key=%s,val=%s", key, val);

        /* We have a key and value parsed out at ths point. */

        if (strcmp("name", key) == 0) {
            if (name != NULL) {
                pdebug(PLCTAG_DEBUG_WARN, "Overwriting attribute %s", "name");
            }
            name = strdup(val);
            if (name == NULL) {
                err(1, "strdup");
            }
        } else if (strcmp("elem_size", key) == 0) {
            if (elem_size > 0) {
                pdebug(PLCTAG_DEBUG_WARN, "Overwriting attribute %s", "elem_size");
            }
            elem_size = atoi(val);
        } else if (strcmp("elem_count", key) == 0) {
            if (elem_count > 0) {
                pdebug(PLCTAG_DEBUG_WARN, "Overwriting attribute %s", "elem_count");
            }
            elem_count = atoi(val);
        }
    }

    if (name == NULL) {
        pdebug(PLCTAG_DEBUG_WARN, "Missing attribute %s", "name");
        ret = PLCTAG_ERR_BAD_PARAM;
        goto done;
    }

    tag = tag_tree_create_node();
    if (tag == NULL) {
        err(1, "tag_tree_create_node");
    }

    MTX_LOCK(&tag->mtx);
    tag->name = name;
    tag->elem_count = elem_count;
    tag->elem_size = elem_size;

    tag->data = calloc(tag->elem_count, tag->elem_size);
    if (tag->data == NULL) {
        err(1, "calloc");
    }

    ret = tag->tag_id;

    MTX_UNLOCK(&tag->mtx);

done:
    free(str);
    return ret;
}

int
plc_tag_destroy(int32_t tag) {
    return tag_tree_remove(tag);
}

int
plc_tag_get_size(int32_t id)
{
    int size;

    struct tag_tree_node* t;

    t = tag_tree_lookup(id);
    if (!t) {
        pdebug(PLCTAG_DEBUG_WARN, "Unknown tag %d", id);
        return PLCTAG_ERR_NOT_FOUND;
    }
    MTX_LOCK(&t->mtx);
    size = t->elem_size * t->elem_size;
    MTX_UNLOCK(&t->mtx);

    return size;
}

/* Stubs out the tag read path.  Only checks that the arguments
 * are valid.  It might be interesting to stub out "in-flight"
 * reads and writes for a heavily-concurrent integration test
 * but I suspect that isn't worth our effort.
 */
int
plc_tag_read(int32_t tag_id, int timeout)
{
    (void)(timeout);
    struct tag_tree_node* t;

    if (timeout < 0) {
        pdebug(PLCTAG_DEBUG_WARN, "Timeout must not be negative");
        return PLCTAG_ERR_BAD_PARAM;
    }

    t = tag_tree_lookup(tag_id);
    if (!t) {
        pdebug(PLCTAG_DEBUG_WARN, "Unknown tag %d", tag_id);
        return PLCTAG_ERR_NOT_FOUND;
    }

    MTX_LOCK(&t->mtx);
    if (t->cb) {
        pdebug(PLCTAG_DEBUG_SPEW,
            "Calling cb for %d with PLCTAG_READ_EVENT_STARTED", tag_id);
        t->cb(tag_id, PLCTAG_EVENT_READ_STARTED, PLCTAG_STATUS_OK);
        pdebug(PLCTAG_DEBUG_SPEW,
            "Calling cb for %d with PLCTAG_READ_EVENT_COMPLETED", tag_id);
        t->cb(tag_id, PLCTAG_EVENT_READ_COMPLETED, PLCTAG_STATUS_OK);
    }
    MTX_UNLOCK(&t->mtx);

    return PLCTAG_STATUS_OK;
}

int
plc_tag_register_callback(int32_t tag_id, tag_callback_func cb)
{
    struct tag_tree_node* t;

    t = tag_tree_lookup(tag_id);
    if (!t) {
        pdebug(PLCTAG_DEBUG_WARN, "Unknown tag %d", tag_id);
        return PLCTAG_ERR_NOT_FOUND;
    }

    MTX_LOCK(&t->mtx);
    t->cb = cb;
    MTX_UNLOCK(&t->mtx);

    return PLCTAG_STATUS_OK;
}

void
plc_tag_set_debug_level(int level)
{
    debug_set_level(level);
}

int
plc_tag_status(int32_t tag)
{
    struct tag_tree_node* t;

    t = tag_tree_lookup(tag);
    if (!t) {
        pdebug(PLCTAG_DEBUG_WARN, "Unknown tag %d", tag);
        return PLCTAG_ERR_NOT_FOUND;
    }

    /* For the stub, let's always just treat the tag status as 
     * okay.  If we stub out in-flight reads and writes later on,
     * this would change.
     */
    return PLCTAG_STATUS_OK;
}

int
plc_tag_unregister_callback(int32_t tag_id)
{
    return plc_tag_register_callback(tag_id, NULL);
}

/* Stubs out the tag write path.
 */
int
plc_tag_write(int32_t tag_id, int timeout)
{
    (void)(timeout);
    struct tag_tree_node* t;

    if (timeout < 0) {
        pdebug(PLCTAG_DEBUG_WARN, "Timeout must not be negative");
        return PLCTAG_ERR_BAD_PARAM;
    }

    t = tag_tree_lookup(tag_id);
    if (!t) {
        pdebug(PLCTAG_DEBUG_WARN, "Unknown tag %d", tag_id);
        return PLCTAG_ERR_NOT_FOUND;
    }

    MTX_LOCK(&t->mtx);
    if (t->cb) {
        pdebug(PLCTAG_DEBUG_SPEW,
            "Calling cb for %d with PLCTAG_WRITE_EVENT_STARTED", tag_id);
        t->cb(tag_id, PLCTAG_EVENT_WRITE_STARTED, PLCTAG_STATUS_OK);
        pdebug(PLCTAG_DEBUG_SPEW,
            "Calling cb for %d with PLCTAG_WRITE_EVENT_COMPLETED", tag_id);
        t->cb(tag_id, PLCTAG_EVENT_WRITE_COMPLETED, PLCTAG_STATUS_OK);
    }
    MTX_UNLOCK(&t->mtx);

    return PLCTAG_STATUS_OK;
}

/* macro expansions */

#define X(name, type) SETTER(name, type);
TYPEMAP
#undef X
#define X(name, type) GETTER(name, type);
TYPEMAP
#undef X
