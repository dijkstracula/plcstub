/* plcstub.c
 *
 * Routines for the top-level interface to the PLC.
 * author: ntaylor
 */

#include <err.h>
#include <inttypes.h>
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
#include "types.h"

/* TODO: there should be a way of unifying these (as well as the _impl functions). */
typedef void(getter_fn)(char* buf, int offset, void* val);
typedef void(setter_fn)(char* buf, int offset, void* val);

/* Accessor / mutator macros */

/*
 * TODO: To allow returning negative values for error codes from
 * plcstub_get_impl, we may have to look at widening the types underlying each
 * particular type.  Not sure how to do that and maintain API compatability with
 * libplctag, though.
 */
#define GETTER(name, type, fprintf_type)                                            \
    static void                                                                     \
        plcstub_##name##_getter_cb(char* buf, int offset, void* val)                \
    {                                                                               \
        type* p = (type*)(buf + offset);                                            \
        *(type*)(val) = *p;                                                         \
        pdebug(PLCTAG_DEBUG_SPEW, "reading at offset %d (%" fprintf_type ")",       \
            offset, *p);                                                            \
    }                                                                               \
    type                                                                            \
        plc_tag_get_##name(int32_t tag, int offset)                                 \
    {                                                                               \
        int impl_ret;                                                               \
        type val;                                                                   \
        impl_ret = plcstub_get_impl(tag, offset, &val, plcstub_##name##_getter_cb); \
        if (impl_ret != PLCTAG_STATUS_OK) {                                         \
            return (type)(impl_ret);                                                \
        }                                                                           \
        return val;                                                                 \
    }

#define SETTER(name, type, fprintf_type)                                        \
    static void                                                                 \
        plcstub_##name##_setter_cb(char* buf, int offset, void* val)            \
    {                                                                           \
        type* p = (type*)(buf + offset);                                        \
        *p = *(type*)(val);                                                     \
        pdebug(PLCTAG_DEBUG_SPEW, "writing at offset %d (%" fprintf_type ")",   \
            offset, *p);                                                        \
    }                                                                           \
    int                                                                         \
        plc_tag_set_##name(int32_t tag, int offset, type val)                   \
    {                                                                           \
        return plcstub_set_impl(tag, offset, &val, plcstub_##name##_setter_cb); \
    }

#define TYPEMAP                       \
    /* X(name, type, fprintf_type) */ \
    X(bit, int, PRId32)               \
    X(uint64, uint64_t, PRIu64)       \
    X(int64, int64_t, PRId64)         \
    X(uint32, uint32_t, PRIu32)       \
    X(int32, int32_t, PRId32)         \
    X(uint16, uint16_t, PRIu16)       \
    X(int16, int16_t, PRId16)         \
    X(uint8, uint8_t, PRIu8)          \
    X(int8, int8_t, PRId8)            \
    X(float64, double, "%f")          \
    X(float32, float, "%f")

static int
plcstub_get_impl(int32_t tag, int offset, void* buf, getter_fn fn)
{
    struct tag_tree_node* t;
    size_t sz;

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

    if (type_to_enum(t->type) != TAG_ARRAY) {
        if (offset > 0) {
            pdebug(PLCTAG_DEBUG_WARN,
                "Offset %d specified for non-array type %s", offset, type_str(t->type));
            if (t->cb) {
                pdebug(PLCTAG_DEBUG_SPEW,
                    "Calling cb for %d with PLCTAG_EVENT_ABORTED", tag);
                t->cb(tag, PLCTAG_EVENT_ABORTED, PLCTAG_ERR_BAD_PARAM);
            }
            MTX_UNLOCK(&t->mtx);
            return PLCTAG_ERR_BAD_PARAM;
        }
    } else {
        struct tag_array* a = (struct tag_array*)(t->type);
        if (offset >= a->len) {
            pdebug(PLCTAG_DEBUG_WARN,
                "Offset %d not in [0, %d)", offset, a->len);
            MTX_UNLOCK(&t->mtx);
            return PLCTAG_ERR_BAD_PARAM;
        }
        offset = offset * type_size_bytes(a->member_type);
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
plcstub_set_impl(int32_t tag, int offset, void* value, setter_fn fn)
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

    if (type_to_enum(t->type) != TAG_ARRAY) {
        if (offset > 0) {
            pdebug(PLCTAG_DEBUG_WARN,
                "Offset %d specified for non-array type %s", offset, type_str(t->type));
            if (t->cb) {
                pdebug(PLCTAG_DEBUG_SPEW,
                    "Calling cb for %d with PLCTAG_EVENT_ABORTED", tag);
                t->cb(tag, PLCTAG_EVENT_ABORTED, PLCTAG_ERR_BAD_PARAM);
            }
            MTX_UNLOCK(&t->mtx);
            return PLCTAG_ERR_BAD_PARAM;
        }
    } else {
        struct tag_array* a = (struct tag_array*)(t->type);
        if (offset >= a->len) {
            pdebug(PLCTAG_DEBUG_WARN,
                "Offset %d not in [0, %d)", offset, a->len);
            MTX_UNLOCK(&t->mtx);
            return PLCTAG_ERR_BAD_PARAM;
        }
        offset = offset * type_size_bytes(a->member_type);
    }

    fn(t->data, offset, value);

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
plc_tag_check_lib_version(int req_major, int req_minor, int req_patch)
{
    (void)(req_major);
    (void)(req_minor);
    (void)(req_patch);
    return PLCTAG_STATUS_OK;
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
     * 
     * Additionally, two more attributes may be set, that we ignore:
     * 1) elem_size: the width of each element in the tag
     * 2) elem_count: how many elements. (TODO: how does this work with multi-dim arrays?)
     */
    char* name = NULL;

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
            name = val;
        } else if (strcmp("elem_size", key) == 0) {
            pdebug(PLCTAG_DEBUG_WARN, "plcstub dicards attribute %s", "elem_size");
        } else if (strcmp("elem_count", key) == 0) {
            pdebug(PLCTAG_DEBUG_WARN, "plcstub dicards attribute %s", "elem_count");
        }
    }

    if (name == NULL) {
        pdebug(PLCTAG_DEBUG_WARN, "Missing attribute %s", "name");
        ret = PLCTAG_ERR_BAD_PARAM;
        goto done;
    }

    /* XXX: This needs to go away immediately, as it hinges on Nathan's misunderstanding
     * of creating a tag ID. Another patch will be put out that returns a new
     * handle to an existing tag rather than creating a new one. */
    ret = tag_tree_insert(name, type_new_simple(TAG_LINT));

done:
    free(str);
    return ret;
}

const char*
plc_tag_decode_error(int rc)
{
    switch (rc) {
    case PLCTAG_STATUS_PENDING:
        return "PLCTAG_STATUS_PENDING";
    case PLCTAG_STATUS_OK:
        return "PLCTAG_STATUS_OK";
    case PLCTAG_ERR_ABORT:
        return "PLCTAG_ERR_ABORT";
    case PLCTAG_ERR_BAD_CONFIG:
        return "PLCTAG_ERR_BAD_CONFIG";
    case PLCTAG_ERR_BAD_CONNECTION:
        return "PLCTAG_ERR_BAD_CONNECTION";
    case PLCTAG_ERR_BAD_DATA:
        return "PLCTAG_ERR_BAD_DATA";
    case PLCTAG_ERR_BAD_DEVICE:
        return "PLCTAG_ERR_BAD_DEVICE";
    case PLCTAG_ERR_BAD_GATEWAY:
        return "PLCTAG_ERR_BAD_GATEWAY";
    case PLCTAG_ERR_BAD_PARAM:
        return "PLCTAG_ERR_BAD_PARAM";
    case PLCTAG_ERR_BAD_REPLY:
        return "PLCTAG_ERR_BAD_REPLY";
    case PLCTAG_ERR_BAD_STATUS:
        return "PLCTAG_ERR_BAD_STATUS";
    case PLCTAG_ERR_CLOSE:
        return "PLCTAG_ERR_CLOSE";
    case PLCTAG_ERR_CREATE:
        return "PLCTAG_ERR_CREATE";
    case PLCTAG_ERR_DUPLICATE:
        return "PLCTAG_ERR_DUPLICATE";
    case PLCTAG_ERR_ENCODE:
        return "PLCTAG_ERR_ENCODE";
    case PLCTAG_ERR_MUTEX_DESTROY:
        return "PLCTAG_ERR_MUTEX_DESTROY";
    case PLCTAG_ERR_MUTEX_INIT:
        return "PLCTAG_ERR_MUTEX_INIT";
    case PLCTAG_ERR_MUTEX_LOCK:
        return "PLCTAG_ERR_MUTEX_LOCK";
    case PLCTAG_ERR_MUTEX_UNLOCK:
        return "PLCTAG_ERR_MUTEX_UNLOCK";
    case PLCTAG_ERR_NOT_ALLOWED:
        return "PLCTAG_ERR_NOT_ALLOWED";
    case PLCTAG_ERR_NOT_FOUND:
        return "PLCTAG_ERR_NOT_FOUND";
    case PLCTAG_ERR_NOT_IMPLEMENTED:
        return "PLCTAG_ERR_NOT_IMPLEMENTED";
    case PLCTAG_ERR_NO_DATA:
        return "PLCTAG_ERR_NO_DATA";
    case PLCTAG_ERR_NO_MATCH:
        return "PLCTAG_ERR_NO_MATCH";
    case PLCTAG_ERR_NO_MEM:
        return "PLCTAG_ERR_NO_MEM";
    case PLCTAG_ERR_NO_RESOURCES:
        return "PLCTAG_ERR_NO_RESOURCES";
    case PLCTAG_ERR_NULL_PTR:
        return "PLCTAG_ERR_NULL_PTR";
    case PLCTAG_ERR_OPEN:
        return "PLCTAG_ERR_OPEN";
    case PLCTAG_ERR_OUT_OF_BOUNDS:
        return "PLCTAG_ERR_OUT_OF_BOUNDS";
    case PLCTAG_ERR_READ:
        return "PLCTAG_ERR_READ";
    case PLCTAG_ERR_REMOTE_ERR:
        return "PLCTAG_ERR_REMOTE_ERR";
    case PLCTAG_ERR_THREAD_CREATE:
        return "PLCTAG_ERR_THREAD_CREATE";
    case PLCTAG_ERR_THREAD_JOIN:
        return "PLCTAG_ERR_THREAD_JOIN";
    case PLCTAG_ERR_TIMEOUT:
        return "PLCTAG_ERR_TIMEOUT";
    case PLCTAG_ERR_TOO_LARGE:
        return "PLCTAG_ERR_TOO_LARGE";
    case PLCTAG_ERR_TOO_SMALL:
        return "PLCTAG_ERR_TOO_SMALL";
    case PLCTAG_ERR_UNSUPPORTED:
        return "PLCTAG_ERR_UNSUPPORTED";
    case PLCTAG_ERR_WINSOCK:
        return "PLCTAG_ERR_WINSOCK";
    case PLCTAG_ERR_WRITE:
        return "PLCTAG_ERR_WRITE";
    case PLCTAG_ERR_PARTIAL:
        return "PLCTAG_ERR_PARTIAL";
    case PLCTAG_ERR_BUSY:
        return "PLCTAG_ERR_BUSY";
    }

    return "Unknown error.";
}

int
plc_tag_destroy(int32_t tag)
{
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
    size = t;
    MTX_UNLOCK(&t->mtx);

    return size;
}

extern int
plc_tag_lock(int32_t id)
{
    struct tag_tree_node* t;

    t = tag_tree_lookup(id);
    if (!t) {
        pdebug(PLCTAG_DEBUG_WARN, "Unknown tag %d", id);
        return PLCTAG_ERR_NOT_FOUND;
    }
    MTX_LOCK(&t->mtx);

    return PLCTAG_STATUS_OK;
}

extern int
plc_tag_unlock(int32_t id)
{
    struct tag_tree_node* t;

    t = tag_tree_lookup(id);
    if (!t) {
        pdebug(PLCTAG_DEBUG_WARN, "Unknown tag %d", id);
        return PLCTAG_ERR_NOT_FOUND;
    }
    MTX_UNLOCK(&t->mtx);

    return PLCTAG_STATUS_OK;
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

#define X(name, type, fprintf_type) SETTER(name, type, fprintf_type);
TYPEMAP
#undef X
#define X(name, type, fprintf_type) GETTER(name, type, fprintf_type);
TYPEMAP
#undef X
