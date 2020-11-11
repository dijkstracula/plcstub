/* plcstub.c
 *
 * Debug log output handlers.
 * author: ntaylor
 */

#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>

#include "debug.h"

/* 
 * Ensures mutual exclusion on the debug log output stream (currently just
 * stderr, but the libplctag API allows setting it elsewhere).
 */
static pthread_mutex_t debug_mtx = PTHREAD_MUTEX_INITIALIZER;

#ifdef DEBUG
volatile static int debug_level = PLCTAG_DEBUG_SPEW;
#else
volatile static int debug_level = PLCTAG_DEBUG_INFO;
#endif

const char*
debug_level_str(int level)
{
    switch (level) {
    case PLCTAG_DEBUG_NONE:
        return "<none>";
    case PLCTAG_DEBUG_ERROR:
        return "ERROR";
    case PLCTAG_DEBUG_WARN:
        return "WARN";
    case PLCTAG_DEBUG_INFO:
        return "INFO";
    case PLCTAG_DEBUG_DETAIL:
        return "DETAIL";
    case PLCTAG_DEBUG_SPEW:
        return "SPEW";
    default:
        return "???";
    }
}

void
pdebug_impl(const char* func, const char* file, int line, int level, const char* msg, ...)
{
    va_list va;
    va_start(va, msg);
    int ret;

    if ((ret = pthread_mutex_lock(&debug_mtx)) != 0) {
        err(1, "pthread_mutex_lock");
    }

    fprintf(stderr, "plcstub [%s]: %s:%d %s: ", debug_level_str(level), file, line, func);
    vfprintf(stderr, msg, va);
    fprintf(stderr, "\n");

    if ((ret = pthread_mutex_unlock(&debug_mtx)) != 0) {
        err(1, "pthread_mutex_unlock");
    }

    va_end(va);
}

int
debug_get_level()
{
    int level;

    __atomic_load(&debug_level, &level, __ATOMIC_ACQUIRE);

    return level;
}

void
debug_set_level(int level)
{
    if (level < PLCTAG_DEBUG_NONE || level > PLCTAG_DEBUG_SPEW) {
        err(1, "Unknown debug level number %d", level);
    }
    __atomic_store(&debug_level, &level, __ATOMIC_RELEASE);
}