#ifndef _PLCSTUB_H_
#define _PLCSTUB_H_

#include <stdint.h>
#include <sys/tree.h>

#ifdef __APPLE__
typedef uintptr_t __uintptr_t;
#endif

typedef void (*tag_callback_func)(int32_t tag_id, int event, int status);

struct tag {
    RB_ENTRY(tag) rb_entry; 
    int tag_id;
    const char *name; /* TODO: TAG_BASE_STRUCT doesn't contain a name: where does the name live? */
    tag_callback_func cb;
};

#endif