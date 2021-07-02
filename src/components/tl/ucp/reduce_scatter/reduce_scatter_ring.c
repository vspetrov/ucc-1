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
#include "coll_patterns/ring.h"

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
    ucc_rank_t             rank      = task->subset.myrank;
    ucc_rank_t             sendto     = (rank + 1) % size;
    ucc_rank_t             recvfrom   = (rank - 1 + size) % size;
    ucc_rank_t         prevblock, recv_data_from;
    ucc_status_t       status;
    size_t              max_real_segsize, block_count, block_offset, frag_count, frag_offset;
    int step, inbi;
    void *inbuf[3];


    sendto   = ucc_ep_map_eval(task->reduce_scatter_ring.inv_map, sendto);
    recvfrom = ucc_ep_map_eval(task->reduce_scatter_ring.inv_map, recvfrom);
    max_real_segsize = ucc_ring_block_count(count, size, 0) * dt_size;
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
        block_offset = ucc_ring_block_offset(count, size, prevblock);
        block_count = ucc_ring_block_count(count, size, prevblock);
        frag_count = ucc_ring_block_count(block_count, task->reduce_scatter_ring.n_frags, task->reduce_scatter_ring.frag);
        frag_offset = ucc_ring_block_offset(block_count, task->reduce_scatter_ring.n_frags, task->reduce_scatter_ring.frag);        
        /* printf("reduce inbi %d, offset %zd count %zd\n", */
               /* inbi ^ 0x1, block_offset, block_count); */
        if (1) {
            if (UCC_OK != (status = ucc_dt_reduce_multi_nb(
                               inbuf[inbi ^ 0x1], PTR_OFFSET(sbuf, (block_offset + frag_offset) * dt_size),
                               inbuf[2], 1, frag_count, 0, dt,
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
        block_offset = ucc_ring_block_offset(count, size, prevblock);
        block_count = ucc_ring_block_count(count, size, prevblock);
        frag_count = ucc_ring_block_count(block_count, task->reduce_scatter_ring.n_frags, task->reduce_scatter_ring.frag);
        frag_offset = ucc_ring_block_offset(block_count, task->reduce_scatter_ring.n_frags, task->reduce_scatter_ring.frag);        

        if (task->send_posted < size) {
            /* printf("PROGRESS: rank %d, sendto %d, len %zd, inbi %d, buf %p\n", */
                   /* rank, sendto, block_count * dt_size, inbi, inbuf[2]); */

            UCPCHECK_GOTO(
                ucc_tl_ucp_send_nb(inbuf[2],  frag_count * dt_size, mem_type, sendto, team, task),
                task, out);
        }

        if (task->recv_posted < size) {
            inbi = (step + 1) % 2;
            recv_data_from = (rank + size - (step + 1) - 1) % size;
            recv_data_from = ucc_ep_map_eval(task->reduce_scatter_ring.inv_map, recv_data_from);
            
            block_offset = ucc_ring_block_offset(count, size, recv_data_from);
            block_count = ucc_ring_block_count(count, size, recv_data_from);
            frag_count = ucc_ring_block_count(block_count, task->reduce_scatter_ring.n_frags, task->reduce_scatter_ring.frag);
            frag_offset = ucc_ring_block_offset(block_count, task->reduce_scatter_ring.n_frags, task->reduce_scatter_ring.frag);        

            /* printf("PROGRESS: rank %d, recv from %d, len %zd, inbi %d, buf %p\n", */
                   /* rank, recvfrom, rlen, inbi, inbuf[inbi]); */

            UCPCHECK_GOTO(
                ucc_tl_ucp_recv_nb((task->recv_posted == size - 1) ?
                                   PTR_OFFSET(args->dst.info.buffer, frag_offset * dt_size): inbuf[inbi],
                                   frag_count * dt_size, mem_type, recvfrom, team, task),
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
    ucc_rank_t         rank      = task->subset.myrank;
    ucc_rank_t         sendto     = (rank + 1) % size;
    ucc_rank_t         recvfrom   = (rank - 1 + size) % size;
    size_t             count   = args->src.info.count;
    ucc_datatype_t     dt      = args->src.info.datatype;
    size_t             dt_size = ucc_dt_size(dt);
    ucc_memory_type_t  mem_type  = args->src.info.mem_type;
    ucc_status_t       status;
    size_t             max_real_segsize, block_count, block_offset, frag_count, frag_offset;
    int inbi;
    void *sbuf = coll_task->args.src.info.buffer;
    void *inbuf[2];
    ucc_rank_t send_data_from, recv_data_from;

    /* task->super.super.status = UCC_OK; */
    /* return ucc_task_complete(coll_task); */
    task->super.super.status = UCC_INPROGRESS;
    ucc_tl_ucp_task_reset(task);
    sendto   = ucc_ep_map_eval(task->reduce_scatter_ring.inv_map, sendto);
    recvfrom = ucc_ep_map_eval(task->reduce_scatter_ring.inv_map, recvfrom);

    max_real_segsize = ucc_ring_block_count(count, size, 0) * dt_size;
    inbuf[0]         = task->reduce_scatter_ring.scratch;
    if (size > 2) {
        inbuf[1] = PTR_OFFSET(inbuf[0], max_real_segsize);
    }
    int step = 0;
    recv_data_from = (rank + size - step - 1) % size;
    send_data_from = (rank + 1 + size - step -1) % size;
    recv_data_from = ucc_ep_map_eval(task->reduce_scatter_ring.inv_map, recv_data_from);
    send_data_from = ucc_ep_map_eval(task->reduce_scatter_ring.inv_map, send_data_from);
    
    ucc_assert(task->send_posted == 0);
    inbi = task->send_posted % 2;
    
    block_count = ucc_ring_block_count(count, size, recv_data_from);
    block_offset = ucc_ring_block_offset(count, size, recv_data_from);
    frag_count = ucc_ring_block_count(block_count, task->reduce_scatter_ring.n_frags,
                                      task->reduce_scatter_ring.frag);
    frag_offset = ucc_ring_block_offset(block_count, task->reduce_scatter_ring.n_frags,
                                        task->reduce_scatter_ring.frag);

    /* printf("rank %d, recv from %d, len %zd, inbi %d, buf %p\n", */
           /* rank, recvfrom, rlen, inbi, inbuf[inbi]); */
    UCPCHECK_GOTO(
        ucc_tl_ucp_recv_nb(inbuf[inbi], frag_count * dt_size, mem_type, recvfrom, team, task),
        task, out);

    /* printf("rank %d, sendto %d, len %zd, offset %zd, inbi %d, buf %p\n", */
           /* rank, sendto, block_count * dt_size, block_offset, */
           /* inbi, PTR_OFFSET(sbuf, block_offset * dt_size)); */

    block_count = ucc_ring_block_count(count, size, send_data_from);
    block_offset = ucc_ring_block_offset(count, size, send_data_from);
    frag_count = ucc_ring_block_count(block_count, task->reduce_scatter_ring.n_frags,
                                      task->reduce_scatter_ring.frag);
    frag_offset = ucc_ring_block_offset(block_count, task->reduce_scatter_ring.n_frags,
                                        task->reduce_scatter_ring.frag);

    /* printf("rank %d, recv from %d, len %zd, inbi %d, buf %p\n", */
    
    UCPCHECK_GOTO(
        ucc_tl_ucp_send_nb(PTR_OFFSET(sbuf, (block_offset + frag_offset) * dt_size),
                           frag_count * dt_size, mem_type, sendto, team, task),
        task, out);

    inbi = task->send_posted % 2;

    step = 1;
    recv_data_from = (rank + size - step - 1) % size;
    recv_data_from = ucc_ep_map_eval(task->reduce_scatter_ring.inv_map, recv_data_from);
    
    block_count = ucc_ring_block_count(count, size, recv_data_from);
    block_offset = ucc_ring_block_offset(count, size, recv_data_from);
    frag_count = ucc_ring_block_count(block_count, task->reduce_scatter_ring.n_frags,
                                      task->reduce_scatter_ring.frag);
    frag_offset = ucc_ring_block_offset(block_count, task->reduce_scatter_ring.n_frags,
                                        task->reduce_scatter_ring.frag);

    /* printf("rank %d, recv from %d, len %zd, inbi %d, buf %p\n", */
    /* rank, recvfrom, rlen, inbi, inbuf[inbi]); */

    UCPCHECK_GOTO(
        ucc_tl_ucp_recv_nb(inbuf[inbi], frag_count * dt_size, mem_type, recvfrom, team, task),
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

    if (task->reduce_scatter_ring.inv_map.type != UCC_EP_MAP_FULL) {
        ucc_ep_map_destroy_inverse(&task->reduce_scatter_ring.inv_map);
    }

    ucc_mc_free(task->reduce_scatter_ring.scratch_mc_header);
    return ucc_tl_ucp_coll_finalize(coll_task);
}


ucc_status_t
ucc_tl_ucp_reduce_scatter_ring_init_impl(ucc_base_coll_args_t *coll_args,
                                         ucc_base_team_t      *team,
                                         ucc_coll_task_t     **task_h,
                                         ucc_tl_team_subset_t subset,
                                         int n_frags, int frag)
{
    ucc_tl_ucp_team_t *tl_team = ucc_derived_of(team, ucc_tl_ucp_team_t);
    ucc_rank_t         size    = tl_team->size;
    size_t             count   = coll_args->args.src.info.count;
    ucc_datatype_t     dt      = coll_args->args.src.info.datatype;
    size_t             dt_size = ucc_dt_size(dt);
    ucc_memory_type_t  mem_type  = coll_args->args.src.info.mem_type;
    size_t to_alloc, max_segcount;
    ucc_tl_ucp_task_t *task;
    ucc_status_t       status;

    if (size == 2) {
        return ucc_tl_ucp_reduce_scatter_knomial_init(coll_args, team, task_h);
    }

    task                 = ucc_tl_ucp_init_task(coll_args, team, 0);
    task->super.post     = ucc_tl_ucp_reduce_scatter_ring_start;
    task->super.progress = ucc_tl_ucp_reduce_scatter_ring_progress;
    task->super.finalize = ucc_tl_ucp_reduce_scatter_ring_finalize;
    task->super.ee       = coll_args->ee;
    task->subset         = subset;
    if (task->subset.map.type != UCC_EP_MAP_FULL) {
        status = ucc_ep_map_create_inverse(task->subset.map,
                                           &task->reduce_scatter_ring.inv_map);
        if (UCC_OK != status) {
            return status;
        }
    } else {
        task->reduce_scatter_ring.inv_map.type = UCC_EP_MAP_FULL;
        task->reduce_scatter_ring.inv_map.ep_num = task->subset.map.ep_num;
    }
    task->reduce_scatter_ring.n_frags = n_frags;
    task->reduce_scatter_ring.frag = frag;

    max_segcount = ucc_ring_block_count(count, size, 0);
    to_alloc = max_segcount + (size > 2 ? 2*max_segcount : 0);
    status = ucc_mc_alloc(&task->reduce_scatter_ring.scratch_mc_header,
                          to_alloc * dt_size, mem_type);

    task->reduce_scatter_ring.scratch =
        task->reduce_scatter_ring.scratch_mc_header->addr;
    *task_h = &task->super;
    return status;
}

ucc_status_t ucc_tl_ucp_reduce_scatter_ring_sched_post(ucc_coll_task_t *coll_task)
{
    ucc_schedule_t *schedule = ucc_derived_of(coll_task, ucc_schedule_t);
    return ucc_schedule_start(schedule);
}

ucc_status_t
ucc_tl_ucp_reduce_scatter_ring_sched_finalize(ucc_coll_task_t *task)
{
    ucc_schedule_t *schedule = ucc_derived_of(task, ucc_schedule_t);
    ucc_status_t status;
    status = ucc_schedule_finalize(task);
    ucc_tl_ucp_put_schedule(schedule);
    return status;
}

ucc_status_t ucc_tl_ucp_reduce_scatter_ring_init(ucc_base_coll_args_t *coll_args,
                                            ucc_base_team_t *     team,
                                            ucc_coll_task_t **    task_h)
{
    const int n_subsets = 3;
    ucc_tl_ucp_team_t *tl_team = ucc_derived_of(team, ucc_tl_ucp_team_t);
    ucc_coll_task_t *ctask;
    ucc_status_t status;
    ucc_tl_team_subset_t subsets[n_subsets];
    ucc_schedule_t *schedule = ucc_tl_ucp_get_schedule(&coll_args->args, tl_team);
    int i;

    subsets[0].myrank = tl_team->rank;
    subsets[0].map.type = UCC_EP_MAP_FULL;
    subsets[0].map.ep_num = tl_team->size;

    subsets[1].map = ucc_ep_map_create_reverse(tl_team->size);
    subsets[1].myrank = ucc_ep_map_eval(subsets[1].map, tl_team->rank);

    static ucc_rank_t array[8] = {1, 4, 2, 7, 5, 0, 6, 3};
    subsets[2].map.type = UCC_EP_MAP_ARRAY;
    subsets[2].map.ep_num = tl_team->size;
    subsets[2].map.array.map = array;
    subsets[2].map.array.elem_size = sizeof(ucc_rank_t);
    subsets[2].myrank = ucc_ep_map_eval(subsets[2].map, tl_team->rank);


    for (i = 0; i < n_subsets; i++) {
        status = ucc_tl_ucp_reduce_scatter_ring_init_impl(coll_args, team, &ctask,
                                                          subsets[i], n_subsets, i);
        if (UCC_OK != status) {
            tl_error(UCC_TL_TEAM_LIB(tl_team), "failed to allocate ring task");
            return status;
        }
        ctask->n_deps = 1;
        ucc_schedule_add_task(schedule, ctask);
        ucc_event_manager_subscribe(&schedule->super.em, UCC_EVENT_SCHEDULE_STARTED,
                                    ctask, ucc_task_start_handler);
    }
    schedule->super.post = ucc_tl_ucp_reduce_scatter_ring_sched_post;
    schedule->super.finalize = ucc_tl_ucp_reduce_scatter_ring_sched_finalize;
    *task_h = &schedule->super;
    return UCC_OK;
}
