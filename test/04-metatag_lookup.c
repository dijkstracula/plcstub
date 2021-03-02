#include <err.h>
#include <stdio.h>

#include "debug.h"
#include "libplctag.h"
#include "plcstub.h"
#include "tagtree.h"

/* This is the name of the first tag that will be reported back. */
#define TAG_NAME_LENGTH (uint16_t)(strlen("DUMMY_AQUA_DATA_0"))

int
main(int argc, char** argv)
{
    int ret, offset;
    int16_t s2;
    int32_t s4;

    plc_tag_set_debug_level(PLCTAG_DEBUG_SPEW);

    ret = plc_tag_read(METATAG_ID, 1000);
    if (ret != PLCTAG_STATUS_OK) {
        errx(1, "plc_tag_read(METATAG_ID, 1000) returned %d", ret);
    }

    offset = 0;

    if ((s4 = plc_tag_get_int32(METATAG_ID, offset)) != 2) {
        errx(1, "Read at offset %d: expected %d, got %d", offset, 2, s4);
    }
    offset += sizeof(s4);

    /* skip over type for now */
    offset += sizeof(s2);

    /* size: TAG_INT == uint16_t */
    if ((s2 = plc_tag_get_int16(METATAG_ID, offset)) != 2) {
        errx(1, "Read at offset %d: expected %d, got %d", offset, 2, s2);
    }
    offset += sizeof(s2);

    /* dims: for a scalar type, this should be 0 */
    if ((s4 = plc_tag_get_int32(METATAG_ID, offset)) != 0) {
        errx(1, "Read at offset %d: expected %d, got %d", offset, 1, s4);
    }
    offset += sizeof(s4) * 3;

    /* length */
    if ((s2 = plc_tag_get_int16(METATAG_ID, offset)) != TAG_NAME_LENGTH) {
        errx(1, "Read at offset %d: expected %d, got %d", offset, TAG_NAME_LENGTH, s2);
    }
    offset += sizeof(s2);

    /* invalid read */
    offset = 1000;
    if ((s2 = plc_tag_get_int16(METATAG_ID, offset)) != PLCTAG_ERR_BAD_PARAM) {
        errx(1, "Read at offset %d: expected %d, got %d", offset, PLCTAG_ERR_BAD_PARAM, s2);
    }

    /* Insert a new tag: we should see that the metatag gets invalidated */
    const char* tag_str = "protocol=ab_eip&gateway=10.206.1.40&path=1,4&cpu=lgx&elem_size=4&elem_count=1&name=TestInsert&debug=4";
    ret = plc_tag_create(tag_str, 1000);
    if (ret < 0) {
        errx(1, "plc_tag_create returned %d", ret);
    } else {
        printf("tag_tree_node creation successful with return value %d\n", ret);
    }

    return 0;
}
