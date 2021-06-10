/**
 * Copyright (C) Mellanox Technologies Ltd. 2021.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */
#include "config.h"
#include "tl_ucp.h"
#include "reduce_scatter.h"
#include "utils/ucc_coll_utils.h"

ucc_base_coll_alg_info_t
    ucc_tl_ucp_reduce_scatter_algs[UCC_TL_UCP_REDUCE_SCATTER_ALG_LAST + 1] = {
        [UCC_TL_UCP_REDUCE_SCATTER_ALG_KNOMIAL] =
            {.id   = UCC_TL_UCP_REDUCE_SCATTER_ALG_KNOMIAL,
             .name = "knomial",
             .desc = "recursive k-ing with arbitrary radix "},
        [UCC_TL_UCP_REDUCE_SCATTER_ALG_LAST] = {
            .id = 0, .name = NULL, .desc = NULL}};