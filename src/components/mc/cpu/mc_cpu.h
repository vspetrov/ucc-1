/**
 * Copyright (C) Mellanox Technologies Ltd. 2020-2021.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#ifndef UCC_MC_CPU_H_
#define UCC_MC_CPU_H_

#include "components/mc/base/ucc_mc_base.h"
#include "components/mc/ucc_mc_log.h"

typedef ucc_status_t (*ucc_mc_cpu_reduce_fn_t)(const void *restrict src1,
                                               const void *restrict src2,
                                               void *restrict dst, size_t nbufs,
                                               size_t count, size_t stride);

typedef struct ucc_mc_cpu_config {
    ucc_mc_config_t super;
} ucc_mc_cpu_config_t;

typedef struct ucc_mc_cpu {
    ucc_mc_base_t super;
    ucc_mc_cpu_reduce_fn_t fns[UCC_DT_USERDEFINED][UCC_OP_USERDEFINED];
} ucc_mc_cpu_t;

extern ucc_mc_cpu_t ucc_mc_cpu;
#endif
