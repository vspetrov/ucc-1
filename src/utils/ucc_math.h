/**
 * Copyright (C) Mellanox Technologies Ltd. 2020-2021.  ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#ifndef UCC_MATH_H_
#define UCC_MATH_H_

#include "config.h"
#include <ucs/sys/math.h>
#include "ucc_datastruct.h"
#include "ucc/api/ucc.h"
#define ucc_min(_a, _b) ucs_min((_a), (_b))
#define ucc_max(_a, _b) ucs_max((_a), (_b))
#define ucc_ilog2(_v)   ucs_ilog2((_v))

extern size_t ucc_dt_sizes[UCC_DT_USERDEFINED];
static inline size_t ucc_dt_size(ucc_datatype_t dt)
{
    if (dt < UCC_DT_USERDEFINED) {
        return ucc_dt_sizes[dt];
    }
    return 0;
}

#define PTR_OFFSET(_ptr, _offset)                                              \
    ((void *)((ptrdiff_t)(_ptr) + (size_t)(_offset)))
#endif
