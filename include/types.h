/* types.h
 */

#ifndef __TYPES_H__
#define __TYPES_H__

#include <stdbool.h>
#include <stdint.h>

/* A type_t is either: 
 * a uintptr_t containing values from TAG_BOOL to TAG_LINT;
 * A pointer to a `struct tag array`, where the first value is TAG_ARRAY;
 * A poitner to a `struct tag_struct`, where the first value is TAG_STRUCT
 */
typedef void* type_t;

/* https://literature.rockwellautomation.com/idc/groups/literature/documents/pm/1756-pm020_-en-p.pdf */
enum tag_type_e {
    TAG_ERROR,
    /* simple types */
    TAG_BOOL,
    TAG_SINT,
    TAG_INT,
    TAG_DINT,
    TAG_REAL,
    TAG_LINT,
    /* complex types */
    TAG_ARRAY,
    TAG_STRUCT,
};

#define BASE_TAG_MEMBERS \
    enum tag_type_e t;

struct tag_array {
    BASE_TAG_MEMBERS

    type_t member_type;
    uint16_t len;
};

struct tag_struct_pair {
    char* name;
    type_t type;
};

struct tag_struct {
    BASE_TAG_MEMBERS

    uint16_t field_cnt;
    struct tag_struct_pair fields[0];
};

enum tag_type_e
type_to_enum(type_t t);

type_t
type_new_simple(enum tag_type_e e);

type_t
type_new_array(uint16_t cnt, type_t member_type);

type_t
type_new_struct(int cnt, ...);

type_t
type_dup(type_t t);

void
type_free(type_t t);

size_t
type_size_bytes(type_t t);

const char*
    type_str(type_t);

bool
type_is_scalar(type_t t);

#endif