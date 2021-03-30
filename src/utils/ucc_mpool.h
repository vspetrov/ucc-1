/**
 * Copyright (C) Mellanox Technologies Ltd. 2021.  ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#ifndef UCC_MPOOL_H_
#define UCC_MPOOL_H_

#include "config.h"
#include <ucs/datastruct/mpool.h>
#include "ucc_compiler_def.h"
#include "ucc_spinlock.h"

typedef struct ucc_mpool {
    ucs_mpool_t pool;
    ucc_thread_mode_t tm;
    ucc_spinlock_t lock;
} ucc_mpool_t;
typedef ucs_mpool_ops_t ucc_mpool_ops_t;

static inline void* ucc_mpool_get(ucc_mpool_t *mp)
{
    void *ret;
    if (UCC_THREAD_MULTIPLE == mp->tm) {
        ucc_spin_lock(&mp->lock);
    }
    ret= ucs_mpool_get(&mp->pool);
    if (UCC_THREAD_MULTIPLE == mp->tm) {
        ucc_spin_unlock(&mp->lock);
    }
    return ret;
}

static inline void ucc_mpool_put(void *obj)
{
    ucs_mpool_elem_t *elem = (ucs_mpool_elem_t*)obj - 1;
    ucc_mpool_t *mp = ucc_container_of(elem->mpool, ucc_mpool_t, pool);
    if (UCC_THREAD_MULTIPLE == mp->tm) {
        ucc_spin_lock(&mp->lock);
    }
    ucs_mpool_put(obj);
    if (UCC_THREAD_MULTIPLE == mp->tm) {
        ucc_spin_unlock(&mp->lock);
    }
}

typedef void (*ucc_mpool_obj_init_fn_t)(ucc_mpool_t *mp, void *obj,
                                        void *chunk);
typedef void (*ucc_mpool_obj_cleanup_fn_t)(ucc_mpool_t *mp, void *obj);

static inline ucc_status_t
ucc_mpool_init(ucc_mpool_t *mp, size_t elem_size, size_t alignment,
               unsigned elems_per_chunk, unsigned max_elems,
               ucc_mpool_obj_init_fn_t    init_fn,
               ucc_mpool_obj_cleanup_fn_t cleanup_fn,
               ucc_thread_mode_t tm, const char *name)
{
    ucs_mpool_ops_t *ops = ucc_malloc(sizeof(*ops), "mpool_ops");
    if (!ops) {
        ucc_error("failed to allocate %zd bytes for mpool ops", sizeof(*ops));
        return UCC_ERR_NO_MEMORY;
    }

    ops->chunk_alloc   = ucs_mpool_hugetlb_malloc;
    ops->chunk_release = ucs_mpool_hugetlb_free;
    ops->obj_init      = (void*)init_fn;
    ops->obj_cleanup   = (void*)cleanup_fn;
    ucc_spinlock_init(&mp->lock, 0);
    mp->tm = tm;
    return ucs_status_to_ucc_status(ucs_mpool_init(
        &mp->pool, 0, elem_size, 0, alignment, elems_per_chunk, max_elems, ops, name));
}

static inline void ucc_mpool_cleanup(ucc_mpool_t *mp, int leak_check)
{
    ucs_mpool_ops_t *ops = mp->pool.data->ops;
    ucs_mpool_cleanup(&mp->pool, leak_check);
    ucc_spinlock_destroy(&mp->lock);
    ucc_free(ops);
}

#endif
