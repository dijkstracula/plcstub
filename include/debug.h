#ifndef _DEBUG_H_
#define _DEBUG_H_

void pdebug_impl(const char *func, const char *file, int line, int level, const char *msg, ...);

#define pdebug(level, ...)                                                   \
    do {                                                                     \
        if ((level) <= get_debug_level()) {                                  \
            pdebug_impl(__FUNC__, __FILE__, __LINE__, (level), __VA_ARGS__); \
        }                                                                    \
    while (0)                                                                \
            
            
#endif
