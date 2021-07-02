/**
 * Copyright (C) Mellanox Technologies Ltd. 2021.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include "config.h"
#include "tl_ucp.h"
#include "allgather.h"
#include "core/ucc_progress_queue.h"
#include "tl_ucp_sendrecv.h"
#include "utils/ucc_math.h"
#include "utils/ucc_coll_utils.h"
#include "core/ucc_mc.h"

#define COMPUTE_BLOCKCOUNT(_count, _num_blocks, _split_index,           \
                           _early_block_count, _late_block_count) do{   \
        _early_block_count = _late_block_count = _count / _num_blocks;  \
        _split_index = _count % _num_blocks;                            \
        if (0 != _split_index) {                                        \
            _early_block_count = _early_block_count + 1;                \
        }                                                               \
    } while(0)

#define GET_RECV_BLOCK_LEN(_rank, _size, _step, _split_index, _early_segcount, _late_segcount) ({ \
            int _recv_block_id = (_rank - (_step+1) + _size) % _size;                         \
            int _recv_len = _recv_block_id < _split_index ? _early_segcount : _late_segcount; \
            _recv_len;                                                            \
        })

ucc_status_t ucc_tl_ucp_allgather_ring_progress(ucc_coll_task_t *coll_task)
{
    ucc_tl_ucp_task_t *task       = ucc_derived_of(coll_task, ucc_tl_ucp_task_t);
    ucc_tl_ucp_team_t *team       = TASK_TEAM(task);
    ucc_rank_t         rank = task->subset.myrank;
    ucc_rank_t         size = (ucc_rank_t)task->subset.map.ep_num;
    void              *rbuf       = coll_task->args.dst.info.buffer;
    ucc_memory_type_t  rmem       = coll_task->args.dst.info.mem_type;
    size_t             count      = coll_task->args.dst.info.count;
    ucc_datatype_t     dt         = coll_task->args.dst.info.datatype;
    ucc_rank_t         sendto     = (rank + 1) % size;
    ucc_rank_t         recvfrom   = (rank - 1 + size) % size;
    int                step;
    void              *buf;
    size_t early_segcount, late_segcount, split_rank, block_count;
    size_t send_block_offset, recv_block_offset;
    ucc_rank_t recv_data_from, send_data_from;

    if (UCC_INPROGRESS == ucc_tl_ucp_test(task)) {
        return task->super.super.status;
    }
    sendto   = ucc_ep_map_eval(task->subset.map, sendto);
    recvfrom = ucc_ep_map_eval(task->subset.map, recvfrom);
    COMPUTE_BLOCKCOUNT(count, size, split_rank, early_segcount, late_segcount);

    while (task->send_posted < size - 1) {
        step = task->send_posted;
        recv_data_from = (rank + size - step - 1) % size;
        send_data_from = (rank + 1 + size - step -1) % size;
        send_block_offset =
            ((send_data_from < split_rank)?
             (send_data_from * early_segcount) :
             (send_data_from * late_segcount + split_rank));
        recv_block_offset =
            ((recv_data_from < split_rank)?
             (recv_data_from * early_segcount) :
             (recv_data_from * late_segcount + split_rank));
        block_count = ((send_data_from < split_rank)?
                       early_segcount : late_segcount);


        buf  = PTR_OFFSET(rbuf, recv_block_offset * ucc_dt_size(dt));
        size_t rlen = GET_RECV_BLOCK_LEN(rank, size, step,
                                         split_rank, early_segcount, late_segcount) * ucc_dt_size(dt);
        UCPCHECK_GOTO(
            ucc_tl_ucp_recv_nb(buf, rlen, rmem, recvfrom, team, task),
            task, out);

        buf  = PTR_OFFSET(rbuf, send_block_offset * ucc_dt_size(dt));
        UCPCHECK_GOTO(
            ucc_tl_ucp_send_nb(buf, block_count*ucc_dt_size(dt), rmem, sendto, team, task),
            task, out);
        if (UCC_INPROGRESS == ucc_tl_ucp_test(task)) {
            return task->super.super.status;
        }
    }
    ucc_assert(UCC_TL_UCP_TASK_P2P_COMPLETE(task));
    task->super.super.status = UCC_OK;
out:
    return task->super.super.status;
}

ucc_status_t ucc_tl_ucp_allgather_ring_start(ucc_coll_task_t *coll_task)
{
    ucc_tl_ucp_task_t *task      = ucc_derived_of(coll_task, ucc_tl_ucp_task_t);
    ucc_tl_ucp_team_t *team      = TASK_TEAM(task);
    size_t             count     = coll_task->args.dst.info.count;
    void              *sbuf      = coll_task->args.src.info.buffer;
    void              *rbuf      = coll_task->args.dst.info.buffer;
    ucc_memory_type_t  smem      = coll_task->args.src.info.mem_type;
    ucc_memory_type_t  rmem      = coll_task->args.dst.info.mem_type;
    ucc_datatype_t     dt        = coll_task->args.dst.info.datatype;
    ucc_status_t       status;

    task->super.super.status     = UCC_INPROGRESS;
    ucc_tl_ucp_task_reset(task);
    if (!UCC_IS_INPLACE(coll_task->args)) {
        size_t early_segcount, late_segcount, split_rank, block_count, block_offset;
        ucc_rank_t rank = task->subset.myrank;

        COMPUTE_BLOCKCOUNT(count, team->size, split_rank, early_segcount, late_segcount);

        block_offset = ((rank < split_rank)? (rank * early_segcount) :
                        (rank * late_segcount + split_rank)) * ucc_dt_size(dt);
        block_count = ((rank < split_rank)? early_segcount : late_segcount);

        status = ucc_mc_memcpy(PTR_OFFSET(rbuf, block_offset),
                               sbuf, block_count*ucc_dt_size(dt), rmem, smem);
        if (ucc_unlikely(UCC_OK != status)) {
            return status;
        }
    }

    status = ucc_tl_ucp_allgather_ring_progress(&task->super);
    if (UCC_INPROGRESS == status) {
        ucc_progress_enqueue(UCC_TL_CORE_CTX(team)->pq, &task->super);
        return UCC_OK;
    }
    return ucc_task_complete(coll_task);
}
