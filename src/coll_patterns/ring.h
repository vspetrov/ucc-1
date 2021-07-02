/**
 * Copyright (C) Mellanox Technologies Ltd. 2021.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#ifndef RING_H_
#define RING_H_

static inline size_t ucc_ring_block_count(size_t total_count, ucc_rank_t n_blocks, ucc_rank_t block)
{
    size_t block_count = total_count / n_blocks;
    size_t left        = total_count % n_blocks;
    return (block < left) ? block_count + 1 : block_count;
}

static inline size_t ucc_ring_block_offset(size_t total_count, ucc_rank_t n_blocks, ucc_rank_t block)
{
    size_t block_count = total_count / n_blocks;
    size_t left        = total_count % n_blocks;
    size_t offset      = block * block_count + left;
    return (block < left) ? offset - (left - block) : offset;
}


#endif
