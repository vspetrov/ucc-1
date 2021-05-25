/**
 * Copyright (C) Mellanox Technologies Ltd. 2020-2021.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#ifndef UCC_MC_CPU_TEMPLATE_H_
#define UCC_MC_CPU_TEMPLATE_H_

#define DECLARE_REDUCE_FN(_type, _op)                                          \
    ucc_status_t ucc_mc_cpu_reduce_##_op##_##_type(                            \
        const void *restrict src1, const void *restrict src2,                  \
        void *restrict dst, size_t nbufs, size_t count, size_t stride)

#define DEFINE_REDUCE_FN_UNROLLED(_type, _op)                                  \
    ucc_status_t ucc_mc_cpu_reduce_##_op##_##_type(                            \
        const void *restrict src1, const void *restrict src2,                  \
        void *restrict dst, size_t nbufs, size_t count, size_t stride)         \
    {                                                                          \
        const _type *restrict s1           = (const _type *restrict)src1;      \
        const _type *restrict s2           = (const _type *restrict)src2;      \
        _type *restrict       d            = (_type * restrict) dst;           \
        size_t                stride_count = stride / sizeof(_type);           \
        int                   i;                                               \
                                                                               \
        switch (nbufs) {                                                       \
        case 1:                                                                \
            for (i = 0; i < count; i++) {                                      \
                d[i] = _op##_1(s1, s2, i, stride_count);                       \
            }                                                                  \
            break;                                                             \
        case 2:                                                                \
            for (i = 0; i < count; i++) {                                      \
                d[i] = _op##_2(s1, s2, i, stride_count);                       \
            }                                                                  \
            break;                                                             \
        case 3:                                                                \
            for (i = 0; i < count; i++) {                                      \
                d[i] = _op##_3(s1, s2, i, stride_count);                       \
            }                                                                  \
            break;                                                             \
        case 4:                                                                \
            for (i = 0; i < count; i++) {                                      \
                d[i] = _op##_4(s1, s2, i, stride_count);                       \
            }                                                                  \
            break;                                                             \
        case 5:                                                                \
            for (i = 0; i < count; i++) {                                      \
                d[i] = _op##_5(s1, s2, i, stride_count);                       \
            }                                                                  \
            break;                                                             \
        case 6:                                                                \
            for (i = 0; i < count; i++) {                                      \
                d[i] = _op##_6(s1, s2, i, stride_count);                       \
            }                                                                  \
            break;                                                             \
        case 7:                                                                \
            for (i = 0; i < count; i++) {                                      \
                d[i] = _op##_7(s1, s2, i, stride_count);                       \
            }                                                                  \
            break;                                                             \
        default:                                                               \
            for (i = 0; i < count; i++) {                                      \
                d[i] = _op##_3(s1, s2, i, stride_count);                       \
            }                                                                  \
            for (i = 1; i < nbufs / 3; i++) {                                  \
                d[i] =                                                         \
                    _op##_3(d, (&s2[3 * i * stride_count]), i, stride_count);  \
            }                                                                  \
            if ((nbufs % 3) == 2) {                                            \
                d[i] = _op##_2(d, (&s2[(nbufs - 2) * i * stride_count]), i,    \
                               stride_count);                                  \
            }                                                                  \
            else if ((nbufs % 3) == 1) {                                       \
                d[i] = _op##_1(d, (&s2[(nbufs - 1) * i * stride_count]), i,    \
                               stride_count);                                  \
            }                                                                  \
            break;                                                             \
        }                                                                      \
        return UCC_OK;                                                         \
    }

#define DEFINE_REDUCE_FN(_type, _op)                                           \
    ucc_status_t ucc_mc_cpu_reduce_##_op##_##_type(                            \
        const void *restrict src1, const void *restrict src2,                  \
        void *restrict dst, size_t nbufs, size_t count, size_t stride)         \
    {                                                                          \
        const _type *restrict s1           = (const _type *restrict)src1;      \
        const _type *restrict s2           = (const _type *restrict)src2;      \
        _type *restrict       d            = (_type * restrict) dst;           \
        size_t                stride_count = stride / sizeof(_type);           \
        const _type *         s;                                               \
        int                   i, j;                                            \
                                                                               \
        for (i = 0; i < count; i++) {                                          \
            d[i] = _op(s1[i], s2[i]);                                          \
        }                                                                      \
        for (j = 1; j < nbufs; j++) {                                          \
            s = s2 + stride_count * j;                                         \
            for (i = 0; i < count; i++) {                                      \
                d[i] = _op(d[i], s[i]);                                        \
            }                                                                  \
        }                                                                      \
        return UCC_OK;                                                         \
    }

#endif
