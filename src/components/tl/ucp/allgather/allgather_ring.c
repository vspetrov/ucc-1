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
#include "coll_patterns/ring.h"
#include "coll_patterns/dgx_rings.h"

ucc_status_t ucc_tl_ucp_allgather_ring_finalize(ucc_coll_task_t *coll_task)
{
    ucc_tl_ucp_task_t *task       = ucc_derived_of(coll_task, ucc_tl_ucp_task_t);
    if (task->allgather_ring.inv_map.type != UCC_EP_MAP_FULL) {
        ucc_ep_map_destroy_inverse(&task->allgather_ring.inv_map);
    }
    return ucc_tl_ucp_coll_finalize(coll_task);
}

ucc_status_t ucc_tl_ucp_allgather_ring_progress(ucc_coll_task_t *coll_task)
{
    ucc_tl_ucp_task_t *task       = ucc_derived_of(coll_task, ucc_tl_ucp_task_t);
    ucc_tl_ucp_team_t *team       = TASK_TEAM(task);
    ucc_rank_t          rank = task->subset.myrank;
    ucc_rank_t         size = (ucc_rank_t)task->subset.map.ep_num;
    void              *rbuf       = coll_task->args.dst.info.buffer;
    ucc_memory_type_t  rmem       = coll_task->args.dst.info.mem_type;
    size_t             count      = coll_task->args.dst.info.count;
    ucc_datatype_t     dt         = coll_task->args.dst.info.datatype;
    ucc_rank_t         sendto     = (rank + 1) % size;
    ucc_rank_t         recvfrom   = (rank - 1 + size) % size;
    int                step;
    void              *buf;
    size_t block_offset, block_count, frag_offset, frag_count;
    ucc_rank_t recv_data_from, send_data_from;

    if (UCC_INPROGRESS == ucc_tl_ucp_test(task)) {
        return task->super.super.status;
    }
    sendto   = ucc_ep_map_eval(task->allgather_ring.inv_map, sendto);
    recvfrom = ucc_ep_map_eval(task->allgather_ring.inv_map, recvfrom);

    while (task->send_posted < size - 1) {
        step = task->send_posted;
        recv_data_from = (rank + size - step - 1) % size;
        send_data_from = (rank + 1 + size - step -1) % size;
        recv_data_from = ucc_ep_map_eval(task->allgather_ring.inv_map, recv_data_from);
        send_data_from = ucc_ep_map_eval(task->allgather_ring.inv_map, send_data_from);


        block_offset = ucc_ring_block_offset(count, size, recv_data_from);
        block_count = ucc_ring_block_count(count, size, recv_data_from);
        frag_offset = ucc_ring_block_offset(block_count, task->allgather_ring.n_frags, task->allgather_ring.frag);
        frag_count = ucc_ring_block_count(block_count, task->allgather_ring.n_frags, task->allgather_ring.frag);

        buf  = PTR_OFFSET(rbuf, (block_offset + frag_offset) * ucc_dt_size(dt));
        UCPCHECK_GOTO(
            ucc_tl_ucp_recv_nb(buf, frag_count*ucc_dt_size(dt), rmem, recvfrom, team, task),
            task, out);

        block_offset = ucc_ring_block_offset(count, size, send_data_from);
        block_count = ucc_ring_block_count(count, size, send_data_from);
        frag_offset = ucc_ring_block_offset(block_count, task->allgather_ring.n_frags, task->allgather_ring.frag);
        frag_count = ucc_ring_block_count(block_count, task->allgather_ring.n_frags, task->allgather_ring.frag);

        buf  = PTR_OFFSET(rbuf, (block_offset + frag_offset) * ucc_dt_size(dt));
        UCPCHECK_GOTO(
            ucc_tl_ucp_send_nb(buf, frag_count*ucc_dt_size(dt), rmem, sendto, team, task),
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
    ucc_rank_t         size      = task->subset.map.ep_num;
    size_t             dt_size   = ucc_dt_size(dt);
    ucc_status_t       status;

    task->super.super.status     = UCC_INPROGRESS;
    ucc_tl_ucp_task_reset(task);
    if (!UCC_IS_INPLACE(coll_task->args)) {
        ucc_rank_t rank = ucc_ep_map_eval(task->allgather_ring.inv_map, task->subset.myrank);
        size_t rank_count  = ucc_ring_block_count(count, size, rank);
        ptrdiff_t rank_offset  = ucc_ring_block_offset(count, size, rank);
        size_t frag_count  = ucc_ring_block_count(rank_count, task->allgather_ring.n_frags, task->allgather_ring.frag);
        ptrdiff_t frag_offset  = ucc_ring_block_offset(rank_count, task->allgather_ring.n_frags, task->allgather_ring.frag);

        status = ucc_mc_memcpy(PTR_OFFSET(rbuf, (rank_offset + frag_offset) * dt_size),
                               PTR_OFFSET(sbuf, frag_offset * dt_size), frag_count*dt_size, rmem, smem);
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


ucc_status_t ucc_tl_ucp_allgather_ring_init_impl(ucc_base_coll_args_t *coll_args,
                                                 ucc_base_team_t *     team,
                                                 ucc_coll_task_t **    task_h,
                                                 ucc_tl_team_subset_t subset,
                                                 int keep_order, int n_frags, int frag)
{
    ucc_tl_ucp_team_t *tl_team = ucc_derived_of(team, ucc_tl_ucp_team_t);
    ucc_tl_ucp_task_t *task;
    ucc_status_t status;
    if ((coll_args->args.src.info.datatype == UCC_DT_USERDEFINED) ||
        (coll_args->args.dst.info.datatype == UCC_DT_USERDEFINED)) {
        tl_error(UCC_TL_TEAM_LIB(tl_team),
                 "user defined datatype is not supported");
        return UCC_ERR_NOT_SUPPORTED;
    }
    task                 = ucc_tl_ucp_init_task(coll_args, team, 0);
    task->super.post     = ucc_tl_ucp_allgather_ring_start;
    task->super.progress = ucc_tl_ucp_allgather_ring_progress;
    task->super.finalize = ucc_tl_ucp_allgather_ring_finalize;
    task->subset = subset;
    if (task->subset.map.type != UCC_EP_MAP_FULL || keep_order) {
        status = ucc_ep_map_create_inverse(task->subset.map,
                                           &task->allgather_ring.inv_map);
        if (UCC_OK != status) {
            return status;
        }
    } else {
        task->allgather_ring.inv_map.type = UCC_EP_MAP_FULL;
        task->allgather_ring.inv_map.ep_num = task->subset.map.ep_num;
    }
    task->allgather_ring.n_frags = n_frags;
    task->allgather_ring.frag = frag;
    *task_h              = &task->super;
    return UCC_OK;
}


ucc_status_t ucc_tl_ucp_allgather_ring_sched_post(ucc_coll_task_t *coll_task)
{
    ucc_schedule_t *schedule = ucc_derived_of(coll_task, ucc_schedule_t);
    int i;
    for (i = 0 ; i < schedule->n_tasks; i++) {
        schedule->tasks[i]->args.src = schedule->super.args.src;
        schedule->tasks[i]->args.dst = schedule->super.args.dst;
    }
    return ucc_schedule_start(schedule);
}

ucc_status_t
ucc_tl_ucp_allgather_ring_sched_finalize(ucc_coll_task_t *task)
{
    ucc_schedule_t *schedule = ucc_derived_of(task, ucc_schedule_t);
    ucc_status_t status;
    status = ucc_schedule_finalize(task);
    ucc_tl_ucp_put_schedule(schedule);
    return status;
}

ucc_status_t ucc_tl_ucp_allgather_ring_init(ucc_base_coll_args_t *coll_args,
                                            ucc_base_team_t *     team,
                                            ucc_coll_task_t **    task_h)
{
    int n_subsets = N_DGX_RINGS;
    ucc_tl_ucp_team_t *tl_team = ucc_derived_of(team, ucc_tl_ucp_team_t);
    ucc_coll_task_t *ctask;
    ucc_status_t status;
    ucc_tl_team_subset_t s;
    ucc_schedule_t *schedule = ucc_tl_ucp_get_schedule(&coll_args->args, tl_team);
    int i;

    /* subsets[0].myrank = tl_team->rank; */
    /* subsets[0].map.type = UCC_EP_MAP_FULL; */
    /* subsets[0].map.ep_num = tl_team->size; */

    /* subsets[1].map = ucc_ep_map_create_reverse(tl_team->size); */
    /* subsets[1].myrank = ucc_ep_map_eval(subsets[1].map, tl_team->rank); */

    /* static ucc_rank_t array[8] = {1, 4, 2, 7, 5, 0, 6, 3}; */
    /* subsets[2].map.type = UCC_EP_MAP_ARRAY; */
    /* subsets[2].map.ep_num = tl_team->size; */
    /* subsets[2].map.array.map = array; */
    /* subsets[2].map.array.elem_size = sizeof(ucc_rank_t); */
    /* subsets[2].myrank = ucc_ep_map_eval(subsets[2].map, tl_team->rank); */

    for (i = 0; i < n_subsets; i++) {
        s = get_dgx_subset(i, tl_team->rank);
        status = ucc_tl_ucp_allgather_ring_init_impl(coll_args, team, &ctask,
                                                     s, 1, n_subsets, i);
        if (UCC_OK != status) {
            tl_error(UCC_TL_TEAM_LIB(tl_team), "failed to allocate ring task");
            return status;
        }
        ctask->n_deps = 1;
        ucc_schedule_add_task(schedule, ctask);
        ucc_event_manager_subscribe(&schedule->super.em, UCC_EVENT_SCHEDULE_STARTED,
                                    ctask, ucc_task_start_handler);
    }
    schedule->super.post = ucc_tl_ucp_allgather_ring_sched_post;
    schedule->super.finalize = ucc_tl_ucp_allgather_ring_sched_finalize;
    *task_h = &schedule->super;
    return UCC_OK;
}
