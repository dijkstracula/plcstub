#include <err.h>
#include <pthread.h>
#include <stdio.h>

#include "debug.h"
#include "libplctag.h"

#define TAGID 4
#define THREADS 16

void*
thread_entry(void* arg)
{
    uint64_t tid = (uintptr_t)arg;
    int ret;

    pdebug(PLCTAG_DEBUG_INFO, "Thread %lu: locking tag %d", tid, TAGID);
    ret = plc_tag_lock(TAGID);
    if (ret != PLCTAG_STATUS_OK) {
        errx(1, "plc_tag_lock(TAGID) returned %s", plc_tag_decode_error(ret));
    }

    pdebug(PLCTAG_DEBUG_INFO, "Thread %lu: unlocking tag %d", tid, TAGID);
    ret = plc_tag_unlock(TAGID);
    if (ret != PLCTAG_STATUS_OK) {
        errx(1, "plc_tag_unlock(TAGID) returned %s", plc_tag_decode_error(ret));
    }

    return NULL;
}

int
main(int argc, char** argv)
{
    int i;

    pthread_t threads[THREADS];

    //plc_tag_set_debug_level(PLCTAG_DEBUG_SPEW);

    for (i = 0; i < sizeof(threads) / sizeof(threads[0]); i++) {
        if (pthread_create(&threads[i], NULL, thread_entry, (void*)(uintptr_t)i)) {
            errx(1, "pthread_create");
        }
    }

    for (i = 0; i < sizeof(threads) / sizeof(threads[0]); i++) {
        if (pthread_join(threads[i], NULL)) {
            errx(1, "pthread_join");
        }
    }

    pdebug(PLCTAG_DEBUG_INFO, "All threads exited.");

    return 0;
}
