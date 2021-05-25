/**
 * Copyright (C) Mellanox Technologies Ltd. 2020-2021.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */
#include "mc_cpu.h"
#include "mc_cpu_template.h"
#define min(_v1, _v2) ((_v1 < _v2) ? _v1 : _v2)

DEFINE_REDUCE_FN(int8_t, min)
DEFINE_REDUCE_FN(int16_t, min)
DEFINE_REDUCE_FN(int32_t, min)
DEFINE_REDUCE_FN(int64_t, min)
DEFINE_REDUCE_FN(uint8_t, min)
DEFINE_REDUCE_FN(uint16_t, min)
DEFINE_REDUCE_FN(uint32_t, min)
DEFINE_REDUCE_FN(uint64_t, min)
DEFINE_REDUCE_FN(float, min)
DEFINE_REDUCE_FN(double, min)
