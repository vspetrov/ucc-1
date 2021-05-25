/**
 * Copyright (C) Mellanox Technologies Ltd. 2020-2021.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include "mc_cpu.h"
#include "mc_cpu_reduce.h"
#include "utils/ucc_malloc.h"
#include <sys/types.h>

static ucc_config_field_t ucc_mc_cpu_config_table[] = {
    {"", "", NULL, ucc_offsetof(ucc_mc_cpu_config_t, super),
     UCC_CONFIG_TYPE_TABLE(ucc_mc_config_table)},

    {NULL}};

static ucc_status_t ucc_mc_cpu_init()
{
    ucc_strncpy_safe(ucc_mc_cpu.super.config->log_component.name,
                     ucc_mc_cpu.super.super.name,
                     sizeof(ucc_mc_cpu.super.config->log_component.name));
    return UCC_OK;
}

static ucc_status_t ucc_mc_cpu_finalize()
{
    return UCC_OK;
}

static ucc_status_t ucc_mc_cpu_mem_alloc(void **ptr, size_t size)
{
    (*ptr) = ucc_malloc(size, "mc cpu");
    if (!(*ptr)) {
        mc_error(&ucc_mc_cpu.super, "failed to allocate %zd bytes", size);
        return UCC_ERR_NO_MEMORY;
    }
    return UCC_OK;
}

static ucc_status_t ucc_mc_cpu_reduce_multi(const void *src1, const void *src2,
                                            void *dst, size_t size,
                                            size_t count, size_t stride,
                                            ucc_datatype_t     dt,
                                            ucc_reduction_op_t op)
{
    return ucc_mc_cpu.fns[dt][op](src1, src2, dst, size, count, stride);
}

static ucc_status_t ucc_mc_cpu_reduce(const void *src1, const void *src2,
                                      void *dst, size_t count,
                                      ucc_datatype_t dt, ucc_reduction_op_t op)
{
    return ucc_mc_cpu_reduce_multi(src1, src2, dst, 1, count, 0, dt, op);
}

static ucc_status_t ucc_mc_cpu_mem_free(void *ptr)
{
    ucc_free(ptr);
    return UCC_OK;
}

static ucc_status_t ucc_mc_cpu_memcpy(void *dst, const void *src, size_t len,
                                      ucc_memory_type_t dst_mem, //NOLINT
                                      ucc_memory_type_t src_mem) //NOLINT
{
    ucc_assert((dst_mem == UCC_MEMORY_TYPE_HOST) &&
               (src_mem == UCC_MEMORY_TYPE_HOST));
    memcpy(dst, src, len);
    return UCC_OK;
}

static ucc_status_t ucc_mc_cpu_mem_query(const void *ptr, size_t length,
                                        ucc_mem_attr_t *mem_attr)
{
    if (ptr == NULL || length == 0) {
        mem_attr->mem_type     = UCC_MEMORY_TYPE_HOST;
        mem_attr->base_address = NULL;
        mem_attr->alloc_length = 0;
        return UCC_OK;
    }

    /* not supposed to be used */
    mc_error(&ucc_mc_cpu.super, "host memory component shouldn't be used for"
                                "mem type detection");
    return UCC_ERR_NOT_SUPPORTED;
}

ucc_status_t ucc_ee_cpu_task_post(void *ee_context, //NOLINT
                                  void **ee_req)
{
    *ee_req = NULL;
    return UCC_OK;
}

ucc_status_t ucc_ee_cpu_task_query(void *ee_req) //NOLINT
{
    return UCC_OK;
}

ucc_status_t ucc_ee_cpu_task_end(void *ee_req) //NOLINT
{
    return UCC_OK;
}

ucc_status_t ucc_ee_cpu_create_event(void **event)
{
    *event = NULL;
    return UCC_OK;
}

ucc_status_t ucc_ee_cpu_destroy_event(void *event) //NOLINT
{
    return UCC_OK;
}

ucc_status_t ucc_ee_cpu_event_post(void *ee_context, //NOLINT
                                   void *event) //NOLINT
{
    return UCC_OK;
}

ucc_status_t ucc_ee_cpu_event_test(void *event) //NOLINT
{
    return UCC_OK;
}

ucc_mc_cpu_t ucc_mc_cpu = {
    .super.super.name       = "cpu mc",
    .super.ref_cnt          = 0,
    .super.type             = UCC_MEMORY_TYPE_HOST,
    .super.ee_type          = UCC_EE_CPU_THREAD,
    .super.init             = ucc_mc_cpu_init,
    .super.finalize         = ucc_mc_cpu_finalize,
    .super.ops.mem_query    = ucc_mc_cpu_mem_query,
    .super.ops.mem_alloc    = ucc_mc_cpu_mem_alloc,
    .super.ops.mem_free     = ucc_mc_cpu_mem_free,
    .super.ops.reduce       = ucc_mc_cpu_reduce,
    .super.ops.reduce_multi = ucc_mc_cpu_reduce_multi,
    .super.ops.memcpy       = ucc_mc_cpu_memcpy,
    .super.config_table =
        {
            .name   = "CPU memory component",
            .prefix = "MC_CPU_",
            .table  = ucc_mc_cpu_config_table,
            .size   = sizeof(ucc_mc_cpu_config_t),
        },
    .super.ee_ops.ee_task_post     = ucc_ee_cpu_task_post,
    .super.ee_ops.ee_task_query    = ucc_ee_cpu_task_query,
    .super.ee_ops.ee_task_end      = ucc_ee_cpu_task_end,
    .super.ee_ops.ee_create_event  = ucc_ee_cpu_create_event,
    .super.ee_ops.ee_destroy_event = ucc_ee_cpu_destroy_event,
    .super.ee_ops.ee_event_post    = ucc_ee_cpu_event_post,
    .super.ee_ops.ee_event_test    = ucc_ee_cpu_event_test,
    .fns                           = {
        [UCC_DT_INT8][UCC_OP_SUM]    = ucc_mc_cpu_reduce_sum_int8_t,
        [UCC_DT_INT16][UCC_OP_SUM]   = ucc_mc_cpu_reduce_sum_int16_t,
        [UCC_DT_INT32][UCC_OP_SUM]   = ucc_mc_cpu_reduce_sum_int32_t,
        [UCC_DT_INT64][UCC_OP_SUM]   = ucc_mc_cpu_reduce_sum_int64_t,
        [UCC_DT_UINT8][UCC_OP_SUM]   = ucc_mc_cpu_reduce_sum_uint8_t,
        [UCC_DT_UINT16][UCC_OP_SUM]  = ucc_mc_cpu_reduce_sum_uint16_t,
        [UCC_DT_UINT32][UCC_OP_SUM]  = ucc_mc_cpu_reduce_sum_uint32_t,
        [UCC_DT_UINT64][UCC_OP_SUM]  = ucc_mc_cpu_reduce_sum_uint64_t,
        [UCC_DT_FLOAT32][UCC_OP_SUM] = ucc_mc_cpu_reduce_sum_float,
        [UCC_DT_FLOAT64][UCC_OP_SUM] = ucc_mc_cpu_reduce_sum_double,

        [UCC_DT_INT8][UCC_OP_PROD]    = ucc_mc_cpu_reduce_prod_int8_t,
        [UCC_DT_INT16][UCC_OP_PROD]   = ucc_mc_cpu_reduce_prod_int16_t,
        [UCC_DT_INT32][UCC_OP_PROD]   = ucc_mc_cpu_reduce_prod_int32_t,
        [UCC_DT_INT64][UCC_OP_PROD]   = ucc_mc_cpu_reduce_prod_int64_t,
        [UCC_DT_UINT8][UCC_OP_PROD]   = ucc_mc_cpu_reduce_prod_uint8_t,
        [UCC_DT_UINT16][UCC_OP_PROD]  = ucc_mc_cpu_reduce_prod_uint16_t,
        [UCC_DT_UINT32][UCC_OP_PROD]  = ucc_mc_cpu_reduce_prod_uint32_t,
        [UCC_DT_UINT64][UCC_OP_PROD]  = ucc_mc_cpu_reduce_prod_uint64_t,
        [UCC_DT_FLOAT32][UCC_OP_PROD] = ucc_mc_cpu_reduce_prod_float,
        [UCC_DT_FLOAT64][UCC_OP_PROD] = ucc_mc_cpu_reduce_prod_double,

        [UCC_DT_INT8][UCC_OP_MAX]    = ucc_mc_cpu_reduce_max_int8_t,
        [UCC_DT_INT16][UCC_OP_MAX]   = ucc_mc_cpu_reduce_max_int16_t,
        [UCC_DT_INT32][UCC_OP_MAX]   = ucc_mc_cpu_reduce_max_int32_t,
        [UCC_DT_INT64][UCC_OP_MAX]   = ucc_mc_cpu_reduce_max_int64_t,
        [UCC_DT_UINT8][UCC_OP_MAX]   = ucc_mc_cpu_reduce_max_uint8_t,
        [UCC_DT_UINT16][UCC_OP_MAX]  = ucc_mc_cpu_reduce_max_uint16_t,
        [UCC_DT_UINT32][UCC_OP_MAX]  = ucc_mc_cpu_reduce_max_uint32_t,
        [UCC_DT_UINT64][UCC_OP_MAX]  = ucc_mc_cpu_reduce_max_uint64_t,
        [UCC_DT_FLOAT32][UCC_OP_MAX] = ucc_mc_cpu_reduce_max_float,
        [UCC_DT_FLOAT64][UCC_OP_MAX] = ucc_mc_cpu_reduce_max_double,

        [UCC_DT_INT8][UCC_OP_MIN]    = ucc_mc_cpu_reduce_min_int8_t,
        [UCC_DT_INT16][UCC_OP_MIN]   = ucc_mc_cpu_reduce_min_int16_t,
        [UCC_DT_INT32][UCC_OP_MIN]   = ucc_mc_cpu_reduce_min_int32_t,
        [UCC_DT_INT64][UCC_OP_MIN]   = ucc_mc_cpu_reduce_min_int64_t,
        [UCC_DT_UINT8][UCC_OP_MIN]   = ucc_mc_cpu_reduce_min_uint8_t,
        [UCC_DT_UINT16][UCC_OP_MIN]  = ucc_mc_cpu_reduce_min_uint16_t,
        [UCC_DT_UINT32][UCC_OP_MIN]  = ucc_mc_cpu_reduce_min_uint32_t,
        [UCC_DT_UINT64][UCC_OP_MIN]  = ucc_mc_cpu_reduce_min_uint64_t,
        [UCC_DT_FLOAT32][UCC_OP_MIN] = ucc_mc_cpu_reduce_min_float,
        [UCC_DT_FLOAT64][UCC_OP_MIN] = ucc_mc_cpu_reduce_min_double,

        [UCC_DT_INT8][UCC_OP_BAND]   = ucc_mc_cpu_reduce_band_int8_t,
        [UCC_DT_INT16][UCC_OP_BAND]  = ucc_mc_cpu_reduce_band_int16_t,
        [UCC_DT_INT32][UCC_OP_BAND]  = ucc_mc_cpu_reduce_band_int32_t,
        [UCC_DT_INT64][UCC_OP_BAND]  = ucc_mc_cpu_reduce_band_int64_t,
        [UCC_DT_UINT8][UCC_OP_BAND]  = ucc_mc_cpu_reduce_band_uint8_t,
        [UCC_DT_UINT16][UCC_OP_BAND] = ucc_mc_cpu_reduce_band_uint16_t,
        [UCC_DT_UINT32][UCC_OP_BAND] = ucc_mc_cpu_reduce_band_uint32_t,
        [UCC_DT_UINT64][UCC_OP_BAND] = ucc_mc_cpu_reduce_band_uint64_t,

        [UCC_DT_INT8][UCC_OP_BOR]   = ucc_mc_cpu_reduce_bor_int8_t,
        [UCC_DT_INT16][UCC_OP_BOR]  = ucc_mc_cpu_reduce_bor_int16_t,
        [UCC_DT_INT32][UCC_OP_BOR]  = ucc_mc_cpu_reduce_bor_int32_t,
        [UCC_DT_INT64][UCC_OP_BOR]  = ucc_mc_cpu_reduce_bor_int64_t,
        [UCC_DT_UINT8][UCC_OP_BOR]  = ucc_mc_cpu_reduce_bor_uint8_t,
        [UCC_DT_UINT16][UCC_OP_BOR] = ucc_mc_cpu_reduce_bor_uint16_t,
        [UCC_DT_UINT32][UCC_OP_BOR] = ucc_mc_cpu_reduce_bor_uint32_t,
        [UCC_DT_UINT64][UCC_OP_BOR] = ucc_mc_cpu_reduce_bor_uint64_t,

        [UCC_DT_INT8][UCC_OP_BXOR]   = ucc_mc_cpu_reduce_bxor_int8_t,
        [UCC_DT_INT16][UCC_OP_BXOR]  = ucc_mc_cpu_reduce_bxor_int16_t,
        [UCC_DT_INT32][UCC_OP_BXOR]  = ucc_mc_cpu_reduce_bxor_int32_t,
        [UCC_DT_INT64][UCC_OP_BXOR]  = ucc_mc_cpu_reduce_bxor_int64_t,
        [UCC_DT_UINT8][UCC_OP_BXOR]  = ucc_mc_cpu_reduce_bxor_uint8_t,
        [UCC_DT_UINT16][UCC_OP_BXOR] = ucc_mc_cpu_reduce_bxor_uint16_t,
        [UCC_DT_UINT32][UCC_OP_BXOR] = ucc_mc_cpu_reduce_bxor_uint32_t,
        [UCC_DT_UINT64][UCC_OP_BXOR] = ucc_mc_cpu_reduce_bxor_uint64_t,

        [UCC_DT_INT8][UCC_OP_LAND]   = ucc_mc_cpu_reduce_land_int8_t,
        [UCC_DT_INT16][UCC_OP_LAND]  = ucc_mc_cpu_reduce_land_int16_t,
        [UCC_DT_INT32][UCC_OP_LAND]  = ucc_mc_cpu_reduce_land_int32_t,
        [UCC_DT_INT64][UCC_OP_LAND]  = ucc_mc_cpu_reduce_land_int64_t,
        [UCC_DT_UINT8][UCC_OP_LAND]  = ucc_mc_cpu_reduce_land_uint8_t,
        [UCC_DT_UINT16][UCC_OP_LAND] = ucc_mc_cpu_reduce_land_uint16_t,
        [UCC_DT_UINT32][UCC_OP_LAND] = ucc_mc_cpu_reduce_land_uint32_t,
        [UCC_DT_UINT64][UCC_OP_LAND] = ucc_mc_cpu_reduce_land_uint64_t,

        [UCC_DT_INT8][UCC_OP_LOR]   = ucc_mc_cpu_reduce_lor_int8_t,
        [UCC_DT_INT16][UCC_OP_LOR]  = ucc_mc_cpu_reduce_lor_int16_t,
        [UCC_DT_INT32][UCC_OP_LOR]  = ucc_mc_cpu_reduce_lor_int32_t,
        [UCC_DT_INT64][UCC_OP_LOR]  = ucc_mc_cpu_reduce_lor_int64_t,
        [UCC_DT_UINT8][UCC_OP_LOR]  = ucc_mc_cpu_reduce_lor_uint8_t,
        [UCC_DT_UINT16][UCC_OP_LOR] = ucc_mc_cpu_reduce_lor_uint16_t,
        [UCC_DT_UINT32][UCC_OP_LOR] = ucc_mc_cpu_reduce_lor_uint32_t,
        [UCC_DT_UINT64][UCC_OP_LOR] = ucc_mc_cpu_reduce_lor_uint64_t,

        [UCC_DT_INT8][UCC_OP_LXOR]   = ucc_mc_cpu_reduce_lxor_int8_t,
        [UCC_DT_INT16][UCC_OP_LXOR]  = ucc_mc_cpu_reduce_lxor_int16_t,
        [UCC_DT_INT32][UCC_OP_LXOR]  = ucc_mc_cpu_reduce_lxor_int32_t,
        [UCC_DT_INT64][UCC_OP_LXOR]  = ucc_mc_cpu_reduce_lxor_int64_t,
        [UCC_DT_UINT8][UCC_OP_LXOR]  = ucc_mc_cpu_reduce_lxor_uint8_t,
        [UCC_DT_UINT16][UCC_OP_LXOR] = ucc_mc_cpu_reduce_lxor_uint16_t,
        [UCC_DT_UINT32][UCC_OP_LXOR] = ucc_mc_cpu_reduce_lxor_uint32_t,
        [UCC_DT_UINT64][UCC_OP_LXOR] = ucc_mc_cpu_reduce_lxor_uint64_t,
    }};

UCC_CONFIG_REGISTER_TABLE_ENTRY(&ucc_mc_cpu.super.config_table,
                                &ucc_config_global_list);
