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

struct __attribute__((packed)) metatag_t {
    uint32_t id;
    uint16_t type;
    uint16_t elem_size;
    uint32_t array_dims[3];
    uint16_t length;
    char data[0];
};

#endif