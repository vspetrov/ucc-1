/**
 * Copyright (C) Mellanox Technologies Ltd. 2021.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include "config.h"
#include "tl_ucp.h"
#include "tl_ucp_coll.h"
#include "tl_ucp_sendrecv.h"
#include "core/ucc_progress_queue.h"
#include "core/ucc_mc.h"
#include "utils/ucc_math.h"
#include "utils/ucc_coll_utils.h"
#include "reduce_scatter.h"

static inline ucc_rank_t ring_rank(ucc_tl_ucp_task_t *task, ucc_rank_t rank) {
    ucc_rank_t r = task->reduce_scatter_ring.backward ?
        task->subset.map.ep_num - rank - 1 : rank;
    return ucc_ep_map_eval(task->subset.map, r);
}

static inline ucc_rank_t my_ring_rank(ucc_tl_ucp_task_t *task) {
    return task->reduce_scatter_ring.backward ?
        task->subset.map.ep_num - task->subset.myrank - 1:
        task->subset.myrank;
}


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
            _recv_len*dt_size;                                                            \
        })

static inline ucc_status_t ucc_tl_ucp_test_ring(ucc_tl_ucp_task_t *task)
{
    int polls = 0;
    if (task->send_posted == task->send_completed &&
        task->recv_posted - task->recv_completed < 2) {
        return UCC_OK;
    }
    while (polls++ < task->n_polls) {
    if (task->send_posted == task->send_completed &&
        task->recv_posted - task->recv_completed < 2) {
            return UCC_OK;
        }
        ucp_worker_progress(TASK_CTX(task)->ucp_worker);
    }
    return UCC_INPROGRESS;
}

ucc_status_t
ucc_tl_ucp_reduce_scatter_ring_progress(ucc_coll_task_t *coll_task)
{
    ucc_tl_ucp_task_t     *task  = ucc_derived_of(coll_task, ucc_tl_ucp_task_t);
    ucc_coll_args_t       *args     = &coll_task->args;
    ucc_tl_ucp_team_t     *team  = TASK_TEAM(task);
    void                  *sbuf      = args->src.info.buffer;
    ucc_memory_type_t      mem_type  = args->src.info.mem_type;
    size_t                 count     = args->src.info.count;
    ucc_datatype_t         dt        = args->src.info.datatype;
    size_t                 dt_size   = ucc_dt_size(dt);
    ucc_rank_t             size      = task->subset.map.ep_num;
    ucc_rank_t             rank      = my_ring_rank(task);
    ucc_rank_t             sendto     = (rank + 1) % size;
    ucc_rank_t             recvfrom   = (rank - 1 + size) % size;
    ucc_rank_t         split_rank, prevblock;
    ucc_status_t       status;
    size_t             early_segcount, late_segcount, rlen, max_real_segsize, block_count, block_offset;
    int step, inbi;
    void *inbuf[3];


    sendto   =ring_rank(task, sendto);
    recvfrom = ring_rank(task, recvfrom);

    COMPUTE_BLOCKCOUNT(count, size, split_rank, early_segcount, late_segcount);
    max_real_segsize = early_segcount * dt_size;
    inbuf[0]         = task->reduce_scatter_ring.scratch;
    if (size > 2) {
        inbuf[1] = PTR_OFFSET(inbuf[0], max_real_segsize);
        inbuf[2] = PTR_OFFSET(inbuf[1], max_real_segsize);
    }
    if (task->reduce_req) {
        if (UCC_OK != ucc_mc_reduce_req_test(task->reduce_req, mem_type)) {
            return task->super.super.status;
        }
        ucc_mc_reduce_req_free(task->reduce_req, mem_type);
        task->reduce_req = NULL;
        goto reduce_done;
    }
    if (UCC_INPROGRESS == ucc_tl_ucp_test_ring(task)) {
        return task->super.super.status;
    }
    while (task->send_posted < size) {
        step = task->send_posted;
        prevblock = (rank + size - step) % size;
        inbi = step % 2;

        /* reduction */
        block_offset = ((prevblock < split_rank)? (prevblock * early_segcount) :
                        (prevblock * late_segcount + split_rank));
        block_count = ((prevblock < split_rank)? early_segcount : late_segcount);

        /* printf("reduce inbi %d, offset %zd count %zd\n", */
               /* inbi ^ 0x1, block_offset, block_count); */
        if (1) {
            if (UCC_OK != (status = ucc_dt_reduce_multi_nb(
                               inbuf[inbi ^ 0x1], PTR_OFFSET(sbuf, block_offset * dt_size),
                               inbuf[2], 1, block_count, 0, dt,
                               mem_type, args, task->super.ee, &task->reduce_req))) {
                tl_error(UCC_TASK_LIB(task), "failed to perform dt reduction");
                task->super.super.status = status;
                return status;
            }
            if (UCC_OK != ucc_mc_reduce_req_test(task->reduce_req, mem_type)) {
                return task->super.super.status;
            }
        }
    reduce_done:
        step = task->send_posted;
        prevblock = (rank + size - step) % size;
        block_count = ((prevblock < split_rank)? early_segcount : late_segcount);

        if (task->send_posted < size) {
            /* printf("PROGRESS: rank %d, sendto %d, len %zd, inbi %d, buf %p\n", */
                   /* rank, sendto, block_count * dt_size, inbi, inbuf[2]); */

            UCPCHECK_GOTO(
                ucc_tl_ucp_send_nb(inbuf[2],  block_count * dt_size, mem_type, sendto, team, task),
                task, out);
        }

        if (task->recv_posted < size) {
            inbi = (step + 1) % 2;
            rlen = GET_RECV_BLOCK_LEN(rank, size, step, split_rank, early_segcount, late_segcount);
            /* printf("PROGRESS: rank %d, recv from %d, len %zd, inbi %d, buf %p\n", */
                   /* rank, recvfrom, rlen, inbi, inbuf[inbi]); */

            UCPCHECK_GOTO(
                ucc_tl_ucp_recv_nb((task->recv_posted == size - 1) ?
                                   args->dst.info.buffer: inbuf[inbi],
                                   rlen, mem_type, recvfrom, team, task),
                task, out);
        }
        if (UCC_INPROGRESS == ucc_tl_ucp_test_ring(task)) {
            return task->super.super.status;
        }
    }
    task->super.super.status = UCC_OK;
out:
    return task->super.super.status;
}

ucc_status_t ucc_tl_ucp_reduce_scatter_ring_start(ucc_coll_task_t *coll_task)
{
    ucc_tl_ucp_task_t *task  = ucc_derived_of(coll_task, ucc_tl_ucp_task_t);
    ucc_coll_args_t   *args  = &coll_task->args;
    ucc_tl_ucp_team_t *team  = TASK_TEAM(task);
    ucc_rank_t         size  = task->subset.map.ep_num;
    ucc_rank_t         rank      = my_ring_rank(task);
    ucc_rank_t         sendto     = (rank + 1) % size;
    ucc_rank_t         recvfrom   = (rank - 1 + size) % size;
    size_t             count   = args->src.info.count;
    ucc_datatype_t     dt      = args->src.info.datatype;
    size_t             dt_size = ucc_dt_size(dt);
    ucc_memory_type_t  mem_type  = args->src.info.mem_type;
    ucc_rank_t         split_rank;
    ucc_status_t       status;
    size_t             early_segcount, late_segcount, rlen, max_real_segsize, block_count, block_offset;
    int inbi;
    void *sbuf = coll_task->args.src.info.buffer;
    void *inbuf[3];

    /* task->super.super.status = UCC_OK; */
    /* return ucc_task_complete(coll_task); */
    task->super.super.status = UCC_INPROGRESS;
    ucc_tl_ucp_task_reset(task);
    sendto   =ring_rank(task, sendto);
    recvfrom = ring_rank(task, recvfrom);

    COMPUTE_BLOCKCOUNT(count, size, split_rank, early_segcount, late_segcount);
    max_real_segsize = early_segcount * dt_size;
    inbuf[0]         = task->reduce_scatter_ring.scratch;
    if (size > 2) {
        inbuf[1] = PTR_OFFSET(inbuf[0], max_real_segsize);
        inbuf[2] = PTR_OFFSET(inbuf[1], max_real_segsize);
    }

    ucc_assert(task->send_posted == 0);
    inbi = task->send_posted % 2;

    rlen = GET_RECV_BLOCK_LEN(rank, size, task->send_posted, split_rank, early_segcount, late_segcount);


    /* printf("rank %d, recv from %d, len %zd, inbi %d, buf %p\n", */
           /* rank, recvfrom, rlen, inbi, inbuf[inbi]); */
    UCPCHECK_GOTO(
        ucc_tl_ucp_recv_nb(inbuf[inbi], rlen, mem_type, recvfrom, team, task),
        task, out);

    block_offset = (rank < split_rank)? (rank * early_segcount) :
        (rank * late_segcount + split_rank);

    block_count = (rank < split_rank)? early_segcount : late_segcount;

    /* printf("rank %d, sendto %d, len %zd, offset %zd, inbi %d, buf %p\n", */
           /* rank, sendto, block_count * dt_size, block_offset, */
           /* inbi, PTR_OFFSET(sbuf, block_offset * dt_size)); */

    UCPCHECK_GOTO(
        ucc_tl_ucp_send_nb(PTR_OFFSET(sbuf, block_offset * dt_size),
                           block_count * dt_size, mem_type, sendto, team, task),
        task, out);

    inbi = task->send_posted % 2;

    rlen = GET_RECV_BLOCK_LEN(rank, size, task->send_posted, split_rank, early_segcount, late_segcount);
    inbi = task->send_posted % 2;
    /* printf("rank %d, recv from %d, len %zd, inbi %d, buf %p\n", */
    /* rank, recvfrom, rlen, inbi, inbuf[inbi]); */

    UCPCHECK_GOTO(
        ucc_tl_ucp_recv_nb(inbuf[inbi], rlen, mem_type, recvfrom, team, task),
        task, out);

    status = ucc_tl_ucp_reduce_scatter_ring_progress(&task->super);
    if (UCC_INPROGRESS == status) {
        ucc_progress_enqueue(UCC_TL_CORE_CTX(team)->pq, &task->super);
        return UCC_OK;
    }
    return ucc_task_complete(coll_task);
out:
    return task->super.super.status;
}

ucc_status_t
ucc_tl_ucp_reduce_scatter_ring_finalize(ucc_coll_task_t *coll_task)
{
    ucc_tl_ucp_task_t *task      = ucc_derived_of(coll_task, ucc_tl_ucp_task_t);

    ucc_mc_free(task->reduce_scatter_ring.scratch_mc_header);
    return ucc_tl_ucp_coll_finalize(coll_task);
}


ucc_status_t
ucc_tl_ucp_reduce_scatter_ring_init(ucc_base_coll_args_t *coll_args,
                                    ucc_base_team_t      *team,
                                    ucc_coll_task_t     **task_h)
{
    ucc_tl_ucp_team_t *tl_team = ucc_derived_of(team, ucc_tl_ucp_team_t);
    ucc_rank_t         size    = tl_team->size;
    size_t             count   = coll_args->args.src.info.count;
    ucc_datatype_t     dt      = coll_args->args.src.info.datatype;
    size_t             dt_size = ucc_dt_size(dt);
    ucc_memory_type_t  mem_type  = coll_args->args.src.info.mem_type;
    size_t             early_segcount, late_segcount, max_real_segsize,
        to_alloc, max_segcount;
    ucc_tl_ucp_task_t *task;
    ucc_rank_t         split_rank;
    ucc_status_t       status;

    if (size == 2) {
        return ucc_tl_ucp_reduce_scatter_knomial_init(coll_args, team, task_h);
    }

    task                 = ucc_tl_ucp_init_task(coll_args, team, 0);
    task->super.post     = ucc_tl_ucp_reduce_scatter_ring_start;
    task->super.progress = ucc_tl_ucp_reduce_scatter_ring_progress;
    task->super.finalize = ucc_tl_ucp_reduce_scatter_ring_finalize;
    task->super.ee       = coll_args->ee;
    task->reduce_scatter_ring.backward = 1;
    COMPUTE_BLOCKCOUNT(count, size, split_rank, early_segcount, late_segcount);
    max_segcount = early_segcount;
    max_real_segsize = max_segcount * dt_size;

    to_alloc = max_real_segsize + (size > 2 ? 2*max_real_segsize : 0);

    status = ucc_mc_alloc(&task->reduce_scatter_ring.scratch_mc_header,
                          to_alloc, mem_type);

    task->reduce_scatter_ring.scratch =
        task->reduce_scatter_ring.scratch_mc_header->addr;
    *task_h = &task->super;
    return status;
}
