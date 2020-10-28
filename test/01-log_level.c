#include <stdio.h>

#include "debug.h"
#include "libplctag.h"

int
main(int argc, char** argv)
{
    plc_tag_set_debug_level(PLCTAG_DEBUG_DETAIL);
    pdebug(PLCTAG_DEBUG_SPEW, "Testing spew level (should not appear)...");
    pdebug(PLCTAG_DEBUG_DETAIL, "Testing detail level (should appear)...");
    pdebug(PLCTAG_DEBUG_INFO, "Testing info level (should appear)...");
}
