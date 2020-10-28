/* plcstub.c
 *
 * Debug log output handlers.
 * author: ntaylor
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <err.h>

#include <pthread.h>

/* TODO(ntaylor): should we just import libplctag? That smells like a circular dependency
 * waiting to happen, but duplicating this is silly too. */
#define PLCTAG_DEBUG_NONE      (0)
#define PLCTAG_DEBUG_ERROR     (1)
#define PLCTAG_DEBUG_WARN      (2)
#define PLCTAG_DEBUG_INFO      (3)
#define PLCTAG_DEBUG_DETAIL    (4)
#define PLCTAG_DEBUG_SPEW      (5)

/* Ensures mutual exclusion on:
 * 1) the log level;
 * 2) the debug log output stream (currently just stderr)
 */
static pthread_mutex_t debug_mtx = PTHREAD_MUTEX_INITIALIZER;

static int debug_level = PLCTAG_DEBUG_INFO;

const char *
debug_level_str(int level) {
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
pdebug_impl(const char *func, const char *file, int line, int level, const char *msg, ...) {
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

int get_debug_level() {
    int level, ret;

    if ((ret = pthread_mutex_lock(&debug_mtx)) != 0) {
        err(1, "pthread_mutex_lock");
    }

    level = debug_level;

    if ((ret = pthread_mutex_unlock(&debug_mtx)) != 0) {
        err(1, "pthread_mutex_unlock");
    }
    
    return ret;
}

void set_debug_level(int level) {
    int ret;

    if ((ret = pthread_mutex_lock(&debug_mtx)) != 0) {
        err(1, "pthread_mutex_lock");
    }

    debug_level = level;

    if ((ret = pthread_mutex_unlock(&debug_mtx)) != 0) {
        err(1, "pthread_mutex_unlock");
    }
}

