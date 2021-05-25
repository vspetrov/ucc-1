/**
 * Copyright (C) Mellanox Technologies Ltd. 2020-2021.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#ifndef UCC_MC_CPU_REDUCE_H_
#define UCC_MC_CPU_REDUCE_H_

#include "mc_cpu.h"
#include "mc_cpu_template.h"

DECLARE_REDUCE_FN(int8_t, sum);
DECLARE_REDUCE_FN(int16_t, sum);
DECLARE_REDUCE_FN(int32_t, sum);
DECLARE_REDUCE_FN(int64_t, sum);
DECLARE_REDUCE_FN(uint8_t, sum);
DECLARE_REDUCE_FN(uint16_t, sum);
DECLARE_REDUCE_FN(uint32_t, sum);
DECLARE_REDUCE_FN(uint64_t, sum);
DECLARE_REDUCE_FN(float, sum);
DECLARE_REDUCE_FN(double, sum);

DECLARE_REDUCE_FN(int8_t, prod);
DECLARE_REDUCE_FN(int16_t, prod);
DECLARE_REDUCE_FN(int32_t, prod);
DECLARE_REDUCE_FN(int64_t, prod);
DECLARE_REDUCE_FN(uint8_t, prod);
DECLARE_REDUCE_FN(uint16_t, prod);
DECLARE_REDUCE_FN(uint32_t, prod);
DECLARE_REDUCE_FN(uint64_t, prod);
DECLARE_REDUCE_FN(float, prod);
DECLARE_REDUCE_FN(double, prod);

DECLARE_REDUCE_FN(int8_t, max);
DECLARE_REDUCE_FN(int16_t, max);
DECLARE_REDUCE_FN(int32_t, max);
DECLARE_REDUCE_FN(int64_t, max);
DECLARE_REDUCE_FN(uint8_t, max);
DECLARE_REDUCE_FN(uint16_t, max);
DECLARE_REDUCE_FN(uint32_t, max);
DECLARE_REDUCE_FN(uint64_t, max);
DECLARE_REDUCE_FN(float, max);
DECLARE_REDUCE_FN(double, max);

DECLARE_REDUCE_FN(int8_t, min);
DECLARE_REDUCE_FN(int16_t, min);
DECLARE_REDUCE_FN(int32_t, min);
DECLARE_REDUCE_FN(int64_t, min);
DECLARE_REDUCE_FN(uint8_t, min);
DECLARE_REDUCE_FN(uint16_t, min);
DECLARE_REDUCE_FN(uint32_t, min);
DECLARE_REDUCE_FN(uint64_t, min);
DECLARE_REDUCE_FN(float, min);
DECLARE_REDUCE_FN(double, min);

DECLARE_REDUCE_FN(int8_t, band);
DECLARE_REDUCE_FN(int16_t, band);
DECLARE_REDUCE_FN(int32_t, band);
DECLARE_REDUCE_FN(int64_t, band);
DECLARE_REDUCE_FN(uint8_t, band);
DECLARE_REDUCE_FN(uint16_t, band);
DECLARE_REDUCE_FN(uint32_t, band);
DECLARE_REDUCE_FN(uint64_t, band);

DECLARE_REDUCE_FN(int8_t, bor);
DECLARE_REDUCE_FN(int16_t, bor);
DECLARE_REDUCE_FN(int32_t, bor);
DECLARE_REDUCE_FN(int64_t, bor);
DECLARE_REDUCE_FN(uint8_t, bor);
DECLARE_REDUCE_FN(uint16_t, bor);
DECLARE_REDUCE_FN(uint32_t, bor);
DECLARE_REDUCE_FN(uint64_t, bor);

DECLARE_REDUCE_FN(int8_t, bxor);
DECLARE_REDUCE_FN(int16_t, bxor);
DECLARE_REDUCE_FN(int32_t, bxor);
DECLARE_REDUCE_FN(int64_t, bxor);
DECLARE_REDUCE_FN(uint8_t, bxor);
DECLARE_REDUCE_FN(uint16_t, bxor);
DECLARE_REDUCE_FN(uint32_t, bxor);
DECLARE_REDUCE_FN(uint64_t, bxor);

DECLARE_REDUCE_FN(int8_t, land);
DECLARE_REDUCE_FN(int16_t, land);
DECLARE_REDUCE_FN(int32_t, land);
DECLARE_REDUCE_FN(int64_t, land);
DECLARE_REDUCE_FN(uint8_t, land);
DECLARE_REDUCE_FN(uint16_t, land);
DECLARE_REDUCE_FN(uint32_t, land);
DECLARE_REDUCE_FN(uint64_t, land);

DECLARE_REDUCE_FN(int8_t, lor);
DECLARE_REDUCE_FN(int16_t, lor);
DECLARE_REDUCE_FN(int32_t, lor);
DECLARE_REDUCE_FN(int64_t, lor);
DECLARE_REDUCE_FN(uint8_t, lor);
DECLARE_REDUCE_FN(uint16_t, lor);
DECLARE_REDUCE_FN(uint32_t, lor);
DECLARE_REDUCE_FN(uint64_t, lor);

DECLARE_REDUCE_FN(int8_t, lxor);
DECLARE_REDUCE_FN(int16_t, lxor);
DECLARE_REDUCE_FN(int32_t, lxor);
DECLARE_REDUCE_FN(int64_t, lxor);
DECLARE_REDUCE_FN(uint8_t, lxor);
DECLARE_REDUCE_FN(uint16_t, lxor);
DECLARE_REDUCE_FN(uint32_t, lxor);
DECLARE_REDUCE_FN(uint64_t, lxor);

#endif
