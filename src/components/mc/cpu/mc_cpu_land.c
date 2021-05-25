/**
 * Copyright (C) Mellanox Technologies Ltd. 2020-2021.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */
#include "mc_cpu.h"
#include "mc_cpu_template.h"

#define land_1(_s1, _s2, _i, _sc) (_s1[_i] && _s2[_i + 0 * _sc])
#define land_2(_s1, _s2, _i, _sc)                                              \
    (_s1[_i] && _s2[_i + 0 * _sc] && _s2[_i + 1 * _sc])
#define land_3(_s1, _s2, _i, _sc)                                              \
    (_s1[_i] && _s2[_i + 0 * _sc] && _s2[_i + 1 * _sc] && _s2[_i + 2 * _sc])
#define land_4(_s1, _s2, _i, _sc)                                              \
    (_s1[_i] && _s2[_i + 0 * _sc] && _s2[_i + 1 * _sc] && _s2[_i + 2 * _sc] && \
     _s2[_i + 3 * _sc])
#define land_5(_s1, _s2, _i, _sc)                                              \
    (_s1[_i] && _s2[_i + 0 * _sc] && _s2[_i + 1 * _sc] && _s2[_i + 2 * _sc] && \
     _s2[_i + 3 * _sc] && _s2[_i + 4 * _sc])
#define land_6(_s1, _s2, _i, _sc)                                              \
    (_s1[_i] && _s2[_i + 0 * _sc] && _s2[_i + 1 * _sc] && _s2[_i + 2 * _sc] && \
     _s2[_i + 3 * _sc] && _s2[_i + 4 * _sc] && _s2[_i + 5 * _sc])
#define land_7(_s1, _s2, _i, _sc)                                              \
    (_s1[_i] && _s2[_i + 0 * _sc] && _s2[_i + 1 * _sc] && _s2[_i + 2 * _sc] && \
     _s2[_i + 3 * _sc] && _s2[_i + 4 * _sc] && _s2[_i + 5 * _sc] &&            \
     _s2[_i + 6 * _sc])

DEFINE_REDUCE_FN_UNROLLED(int8_t, land)
DEFINE_REDUCE_FN_UNROLLED(int16_t, land)
DEFINE_REDUCE_FN_UNROLLED(int32_t, land)
DEFINE_REDUCE_FN_UNROLLED(int64_t, land)

DEFINE_REDUCE_FN_UNROLLED(uint8_t, land)
DEFINE_REDUCE_FN_UNROLLED(uint16_t, land)
DEFINE_REDUCE_FN_UNROLLED(uint32_t, land)
DEFINE_REDUCE_FN_UNROLLED(uint64_t, land)
