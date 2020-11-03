#ifndef _PLCSTUB_H_
#define _PLCSTUB_H_

#include <stdint.h>
#include <sys/tree.h>

#define NTAGS 10

#ifdef __APPLE__
typedef uintptr_t __uintptr_t;
#endif

typedef void (*tag_callback_func)(int32_t tag_id, int event, int status);

/* https://literature.rockwellautomation.com/idc/groups/literature/documents/pm/1756-pm020_-en-p.pdf */
/* TODO: plc_tag_create()'s attribute list consumes a size, not a type.  Do we need this? */
enum tag_type {
    TAG_BOOL,
    TAG_SINT,
    TAG_INT,
    TAG_DINT,
    TAG_REAL,
    TAG_LINT
};

struct tag {
    RB_ENTRY(tag)
    rb_entry;
    int tag_id;
    char* name; /* TODO: TAG_BASE_STRUCT doesn't contain a name: where does the name live? */
    tag_callback_func cb;

    size_t elem_size;
    size_t elem_count;

    /* of length (elem_size * elem_count) */
    char* data;
};

#endif