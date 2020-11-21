#include <stdio.h>

#include "debug.h"
#include "libplctag.h"

#define TAGID 3

void
callback(int32_t tag_id, int event, int status)
{
    pdebug(PLCTAG_DEBUG_INFO, "Callback called with event %d", event);
}

int
main(int argc, char** argv)
{
    plc_tag_set_debug_level(PLCTAG_DEBUG_DETAIL);

    plc_tag_register_callback(TAGID, callback);
    plc_tag_set_int32(TAGID, 0, 0);
    plc_tag_set_int32(TAGID, 0, 1);

    plc_tag_set_int32(42, 0, 1);
}
