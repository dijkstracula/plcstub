#include <err.h>
#include <stdio.h>

#include "debug.h"
#include "libplctag.h"

int
main(int argc, char** argv)
{
    char buf[1024];
    int num_elems = 16;
    int tid = 42;
    int ret;

    plc_tag_set_debug_level(PLCTAG_DEBUG_DETAIL);

    /* Taken from src/examples/stress_test.c in libplctag. */
    const char* tag_str = "protocol=ab_eip&gateway=10.206.1.40&path=1,4&cpu=lgx&elem_size=4&elem_count=%d&name=TestBigArray[%d]&debug=4";
    snprintf(buf, sizeof(buf), tag_str, num_elems, (tid - 1) * num_elems);

    ret = plc_tag_create(buf, 1000);
    if (ret < 0) {
        errx(1, "plc_tag_create returned %d", ret);
    } else {
        printf("tag_tree_node creation successful with return value %d\n", ret);
    }

    printf("Test passed!\n");
    return 0;
}
