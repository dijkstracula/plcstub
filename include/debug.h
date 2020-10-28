#ifndef _DEBUG_H_
#define _DEBUG_H_

/* TODO(ntaylor): should we just import libplctag? That smells like a circular dependency
 * waiting to happen, but duplicating this is silly too. */
#define PLCTAG_DEBUG_NONE (0)
#define PLCTAG_DEBUG_ERROR (1)
#define PLCTAG_DEBUG_WARN (2)
#define PLCTAG_DEBUG_INFO (3)
#define PLCTAG_DEBUG_DETAIL (4)
#define PLCTAG_DEBUG_SPEW (5)

void
pdebug_impl(const char* func, const char* file, int line, int level, const char* msg, ...);

#define pdebug(level, ...)                                                       \
    do {                                                                         \
        if ((level) <= get_debug_level()) {                                      \
            pdebug_impl(__FUNCTION__, __FILE__, __LINE__, (level), __VA_ARGS__); \
        }                                                                        \
    } while (0)

int
get_debug_level(void);
void
set_debug_level(int level);

#endif
