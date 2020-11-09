#ifndef _LOCK_UTILS_H
#define _LOCK_UTILS_H

#include <err.h>
#include <pthread.h>

#define MTX_OP(op, mtx_p)                        \
    do {                                         \
        int ret = op(mtx_p);                     \
        if (ret) {                               \
            errx(1, "%s returned %d", #op, ret); \
        }                                        \
    } while (0)

#define MTX_UNLOCK(mtx_p) MTX_OP(pthread_mutex_unlock, mtx_p)
#define MTX_LOCK(mtx_p) MTX_OP(pthread_mutex_lock, mtx_p)

#define RW_UNLOCK(mtx_p) MTX_OP(pthread_rwlock_unlock, mtx_p)
#define RW_RDLOCK(mtx_p) MTX_OP(pthread_rwlock_rdlock, mtx_p)
#define RW_WRLOCK(mtx_p) MTX_OP(pthread_rwlock_wrlock, mtx_p)

#endif