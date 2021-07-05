/**
 * Copyright (C) Mellanox Technologies Ltd. 2021.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#ifndef DGX_RINGS_H_
#define DGX_RINGS_H_
#include "ucc/api/ucc.h"
#include "utils/ucc_datastruct.h"
#include "components/tl/ucc_tl.h"

#define N_DGX_RINGS_MAX 6

#define N_DGX_RINGS 2

static ucc_rank_t dgx_maps[N_DGX_RINGS_MAX][8]  = {
    {0, 3, 2, 1, 7, 4, 5, 6}, //NV2
    {7, 4, 5, 6, 0, 3, 2, 1}, //NV2 backward
    {0, 3, 2, 1, 7, 4, 5, 6}, //NV2
    {7, 4, 5, 6, 0, 3, 2, 1},  // NV2 backward
    {0, 1, 7, 2, 5, 4, 6, 3}, // NV1
    {7, 6, 0, 5, 2, 3, 1, 4}, //NV1 backward
};

#define INIT_RING(_n)     {\
        .type = UCC_EP_MAP_ARRAY,\
        .ep_num = 8,\
        .array.map = dgx_maps[_n],\
        .array.elem_size = sizeof(ucc_rank_t)\
    }\

static ucc_ep_map_t dgx_rings[N_DGX_RINGS_MAX] = {
    INIT_RING(0),
    INIT_RING(1),
    INIT_RING(2),
    INIT_RING(3),
    INIT_RING(4),
    INIT_RING(5),
};

static inline ucc_tl_team_subset_t get_dgx_subset(int n, ucc_rank_t rank)
{
    ucc_tl_team_subset_t s;
    s.map = dgx_rings[n];
    s.myrank = ucc_ep_map_eval(s.map, rank);
    return s;
}

#endif
