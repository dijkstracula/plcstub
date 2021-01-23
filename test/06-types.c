#include "assert.h"
#include <err.h>
#include <stdio.h>
#include <stdlib.h>

#include "debug.h"
#include "libplctag.h"
#include "types.h"

#define SIMPLE_TYPE(t) (type_t)(uintptr_t)(t)

type_t bool_literal = SIMPLE_TYPE(TAG_BOOL);
type_t sint_literal = SIMPLE_TYPE(TAG_SINT);
type_t int_literal = SIMPLE_TYPE(TAG_INT);
type_t dint_literal = SIMPLE_TYPE(TAG_DINT);
type_t real_literal = SIMPLE_TYPE(TAG_REAL);
type_t lint_literal = SIMPLE_TYPE(TAG_LINT);

type_t nonsense = (void*)("Hello world");

type_t array_of_16_bools;
type_t array_of_7_dints;

type_t struct_of_three_ints;

void
test_tag_type()
{
    assert(type_to_enum(NULL) == TAG_ERROR);
    assert(type_to_enum(nonsense) == TAG_ERROR);

    assert(type_to_enum(bool_literal) == TAG_BOOL);
    assert(type_to_enum(sint_literal) == TAG_SINT);
    assert(type_to_enum(int_literal) == TAG_INT);
    assert(type_to_enum(dint_literal) == TAG_DINT);
    assert(type_to_enum(real_literal) == TAG_REAL);
    assert(type_to_enum(lint_literal) == TAG_LINT);

    assert(type_to_enum(array_of_16_bools) == TAG_ARRAY);
    assert(type_to_enum(array_of_7_dints) == TAG_ARRAY);

    assert(type_to_enum(struct_of_three_ints) == TAG_STRUCT);
}

void
test_tag_size()
{
    assert(type_size_bytes(NULL) == 0);
    assert(type_size_bytes(nonsense) == 0);

    assert(type_size_bytes(bool_literal) == 1);
    assert(type_size_bytes(sint_literal) == 1);
    assert(type_size_bytes(int_literal) == 2);
    assert(type_size_bytes(dint_literal) == 4);
    assert(type_size_bytes(real_literal) == 4);
    assert(type_size_bytes(lint_literal) == 8);

    assert(type_size_bytes(array_of_16_bools) == 16);
    assert(type_size_bytes(array_of_7_dints) == 4 * 7);

    assert(type_size_bytes(struct_of_three_ints) == 2 * 3);
}

void
test_tag_dup()
{
    type_t* t;

    /* non-allocating duplications */
    assert(type_to_enum(type_dup(bool_literal)) == TAG_BOOL);
    assert(type_to_enum(type_dup(sint_literal)) == TAG_SINT);
    assert(type_to_enum(type_dup(int_literal)) == TAG_INT);
    assert(type_to_enum(type_dup(dint_literal)) == TAG_DINT);
    assert(type_to_enum(type_dup(real_literal)) == TAG_REAL);
    assert(type_to_enum(type_dup(lint_literal)) == TAG_LINT);
}

void
init()
{
    array_of_16_bools = type_new_array(16, SIMPLE_TYPE(TAG_BOOL));
    array_of_7_dints = type_new_array(7, SIMPLE_TYPE(TAG_DINT));
    struct_of_three_ints = type_new_struct(3,
        "field_1", SIMPLE_TYPE(TAG_INT),
        "field_2", SIMPLE_TYPE(TAG_INT),
        "field_3", SIMPLE_TYPE(TAG_INT));
}

void
tidy()
{
    type_free(array_of_16_bools);
    type_free(array_of_7_dints);
    type_free(struct_of_three_ints);
}

int
main(int argc, char** argv)
{
    init();

    test_tag_type();
    test_tag_size();

    tidy();

    return 0;
}
