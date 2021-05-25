/**
 * Copyright (C) Mellanox Technologies Ltd. 2020-2021.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */
#include "mc_cpu.h"
#include "mc_cpu_template.h"
#define max(_v1, _v2) ((_v1 > _v2) ? _v1 : _v2)

DEFINE_REDUCE_FN(int8_t, max)
DEFINE_REDUCE_FN(int16_t, max)
DEFINE_REDUCE_FN(int32_t, max)
DEFINE_REDUCE_FN(int64_t, max)
DEFINE_REDUCE_FN(uint8_t, max)
DEFINE_REDUCE_FN(uint16_t, max)
DEFINE_REDUCE_FN(uint32_t, max)
DEFINE_REDUCE_FN(uint64_t, max)
DEFINE_REDUCE_FN(float, max)
DEFINE_REDUCE_FN(double, max)
