/* types.c
 * 
 * Routines for tag typechecking
 */

#include <err.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libplctag.h"
#include "plcstub.h"
#include "types.h"

#define SIMPLE_TYPE(t) (type_t)(uintptr_t)(t)

enum tag_type_e
type_to_enum(type_t t)
{
    uintptr_t as_simple = (uintptr_t)(t);
    enum tag_type_e* as_complex = (enum tag_type_e*)(t);

    if (as_simple <= TAG_LINT) {
        return (enum tag_type_e)(as_simple);
    }
    if (*as_complex == TAG_ARRAY) {
        return TAG_ARRAY;
    }
    if (*as_complex == TAG_STRUCT) {
        return TAG_STRUCT;
    }
    return TAG_ERROR;
}

type_t
type_dup(type_t t)
{
    enum tag_type_e e = type_to_enum(t);

    if (e == TAG_ARRAY) {
        struct tag_array* a = (struct tag_array*)(t);
        return type_new_array(a->len, a->member_type);
    } else if (e == TAG_STRUCT) {
        int i;
        struct tag_struct *s, *new;

        /* XXX: by my own petard, no clean way to call the variadic
         * type_new_struct().  Duplicate the functionality here. */
        s = (struct tag_struct*)(t);
        new = malloc(sizeof(struct tag_struct) + s->field_cnt * sizeof(type_t));
        if (new == NULL) {
            err(1, "malloc");
        }
        new->t = TAG_STRUCT;
        new->field_cnt = s->field_cnt;

        for (i = 0; i < s->field_cnt; i++) {
            new->fields[i].name = strdup(s->fields[i].name);
            new->fields[i].type = type_dup(s->fields[i].type);
        }

        return new;
    }
    /* primitive type; return "copy" by value. */
    return t;
}

type_t
type_new_simple(enum tag_type_e e)
{
    if (e >= TAG_BOOL && e <= TAG_LINT) {
        return (type_t)(e);
    }
    return (type_t)(TAG_ERROR);
}

type_t
type_new_array(uint16_t cnt, type_t member_type)
{
    struct tag_array* a;

    a = malloc(sizeof(struct tag_array));
    if (a == NULL) {
        err(1, "malloc");
    }

    a->t = TAG_ARRAY;
    a->len = cnt;
    a->member_type = type_dup(member_type);

    return a;
}

type_t
type_new_struct(int cnt, ...)
{
    int i;
    struct tag_struct* s;
    va_list ap;
    va_start(ap, cnt);

    s = malloc(sizeof(struct tag_struct) + cnt * sizeof(struct tag_struct_pair));
    if (s == NULL) {
        err(1, "malloc");
    }
    s->t = TAG_STRUCT;
    s->field_cnt = cnt;

    for (i = 0; i < cnt; i++) {
        s->fields[i].name = strdup(va_arg(ap, char*));
        s->fields[i].type = type_dup(va_arg(ap, type_t));
    }

    return s;
}

void
type_free(type_t t)
{
    enum tag_type_e e = type_to_enum(t);
    if (e == TAG_ARRAY) {
        struct tag_array* a = (struct tag_array*)(t);
        type_free(a->member_type);
        free(a);
    }
    if (e == TAG_STRUCT) {
        int i;
        struct tag_struct* s = (struct tag_struct*)(t);
        for (i = 0; i < s->field_cnt; i++) {
            free(s->fields[i].name);
            type_free(s->fields[i].type);
        }
    }
}

size_t
type_size_bytes(type_t t)
{
    enum tag_type_e e = type_to_enum(t);
    switch (e) {
    case TAG_ERROR:
        return 0;
    case TAG_BOOL:
        return 1;
    case TAG_SINT:
        return 1;
    case TAG_INT:
        return 2;
    case TAG_DINT:
        return 4;
    case TAG_REAL:
        return 4;
    case TAG_LINT:
        return 8;
    case TAG_ARRAY: {
        struct tag_array* a = (struct tag_array*)(t);
        return type_size_bytes(a->member_type) * a->len;
    }
    case TAG_STRUCT: {
        int i;
        size_t sz = 0;
        struct tag_struct* s = (struct tag_struct*)(t);
        for (i = 0; i < s->field_cnt; i++) {
            sz += type_size_bytes(s->fields[i].type);
        }
        return sz;
    }
    }
}

bool
type_is_scalar(type_t t)
{
    enum tag_type_e e = type_to_enum(t);
    switch (e) {
    case TAG_ERROR:
    case TAG_BOOL:
    case TAG_SINT:
    case TAG_INT:
    case TAG_DINT:
    case TAG_REAL:
    case TAG_LINT:
        return true;
    case TAG_ARRAY:
    case TAG_STRUCT:
        return false;
    }
}

const char*
type_str(type_t t)
{
    enum tag_type_e e = type_to_enum(t);
    switch (e) {
    case TAG_BOOL:
        return "BOOL";
    case TAG_SINT:
        return "SINT";
    case TAG_INT:
        return "INT";
    case TAG_DINT:
        return "DINT";
    case TAG_REAL:
        return "REAL";
    case TAG_LINT:
        return "LINT";
    case TAG_ARRAY:
        return "ARRAY";
    case TAG_STRUCT:
        return "STRUCT";
    default:
        return "ERROR";
    }
}