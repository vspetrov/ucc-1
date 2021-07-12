
/**
 * Copyright (C) Mellanox Technologies Ltd. 2021.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include "config.h"
#include "tl_ucp.h"
#include "alltoallv.h"
#include "core/ucc_progress_queue.h"
#include "utils/ucc_math.h"
#include "utils/ucc_coll_utils.h"
#include "tl_ucp_sendrecv.h"


ucc_status_t ucc_tl_ucp_alltoallv_imbalanced_finalize(ucc_coll_task_t *coll_task)
{
    ucc_tl_ucp_task_t *task = ucc_derived_of(coll_task, ucc_tl_ucp_task_t);
    ucc_free(task->alltoallv_imbalanced.ready);
    if (task->alltoallv_imbalanced.put_req) {
        ucp_request_free(task->alltoallv_imbalanced.put_req);
    }
    ucc_tl_ucp_put_task(task);
    return UCC_OK;
}

void ucx_empty_complete_cb(void *req, ucs_status_t status)
{
    
}

ucc_status_t ucc_tl_ucp_alltoallv_imbalanced_progress(ucc_coll_task_t *coll_task)
{
    ucc_tl_ucp_task_t *task  = ucc_derived_of(coll_task, ucc_tl_ucp_task_t);
    ucc_tl_ucp_team_t *team  = task->team;
    uint32_t           ra_i  = task->tag % team->n_ra;
    uint32_t          *ready = task->alltoallv_imbalanced.ready;
    ucp_ep_h           ep;
    ucc_status_t       status;
    /* ucs_status_ptr_t   ucs_status; */
    ptrdiff_t          sbuf  = (ptrdiff_t)task->args.src.info_v.buffer;
    ucc_memory_type_t  smem  = task->args.src.info_v.mem_type;
    ucc_rank_t         grank = team->rank;
    ucc_rank_t         gsize = team->size;
    ucc_rank_t         peer, i;
    size_t             sdt_size, data_size, data_displ;

    sdt_size = ucc_dt_size(task->args.dst.info_v.datatype);

    if (ready[gsize + 1 + grank] < gsize) {
        status = ucc_tl_ucp_get_ep(team, team->ra[ra_i].owner, &ep);
        if (ucc_unlikely(UCC_OK != status)) {
            return status;
        }
        ucp_request_param_t param = {
            .op_attr_mask = UCP_OP_ATTR_FIELD_MEMORY_TYPE,            
            .memory_type =  UCS_MEMORY_TYPE_HOST,
        };
        void *request =
            ucp_get_nbx(ep, ready,
                        sizeof(uint32_t) * (team->size + 1),
                        (uintptr_t)team->ra[ra_i].seq_num,
                        team->ra[ra_i].rkey, &param);
        if (UCS_PTR_IS_ERR(request)) {
            tl_error(UCC_TL_TEAM_LIB(team), "failed to put_nbx ra status");
            task->super.super.status = status =
                UCC_ERR_NO_MESSAGE;
        }
        if (request != NULL) {
            ucs_status_t _status;
            do {
                ucp_worker_progress(UCC_TL_UCP_TEAM_CTX(team)->ucp_worker);
                _status = ucp_request_check_status(request);
            } while (_status == UCS_INPROGRESS);
            ucp_request_release(request);
        }
        
        ucc_assert(ready[team->size] == 0xdeadbeef);
        if (0) {
            char str[1024] = "ready: ", tmp[64];
            int j;
            for (j = 0; j < team->size; j++) {
                sprintf(tmp, "%d ", ready[j]);
                strcat(str, tmp);
            }
            fprintf(stderr, "%s\n", str);
        }
        ready[gsize + 1 + grank] = 0;
        for (i = 0; i < gsize; i++) {
            ready[gsize + 1 + grank] += (ready[i] == task->tag) ? 1 : 0;
        }
        ready[gsize] = 0;
    }

    int posts    = UCC_TL_UCP_TEAM_LIB(team)->cfg.alltoallv_pairwise_num_posts;
    posts    = (posts > gsize || posts == 0) ? gsize : posts;

    for (i = grank + 1; (i < gsize + grank) && (task->send_posted - task->send_completed < posts); i++) {
        peer = i % gsize;
        if ((ready[gsize + 1 + peer] == 0) && (ready[peer] == task->tag)) {
            data_size  = ucc_coll_args_get_count(&task->args,
                                                 task->args.src.info_v.counts, peer) * sdt_size;
            data_displ = ucc_coll_args_get_displacement(&task->args,
                                                        task->args.src.info_v.displacements,
                                                        peer) * sdt_size;
            /* fprintf(stderr, "rank %d posting send to peer %d, ready[self] %d, ready peer %d\n", */
            /*         grank, peer, ready[grank], ready[peer]); */
            UCPCHECK_GOTO(ucc_tl_ucp_send_nz((void *)(sbuf + data_displ),
                                             data_size, smem, peer, team, task),
                          task, out);
            ready[gsize + 1 + peer] = 1;
        }
    }
    
    if (task->send_posted < gsize) {
        task->super.super.status = UCC_INPROGRESS;
        return UCC_INPROGRESS;
    }
        
    task->super.super.status = ucc_tl_ucp_test(task);

out:
    return coll_task->super.status;
}

ucc_status_t ucc_tl_ucp_alltoallv_imbalanced_start(ucc_coll_task_t *coll_task)
{
    ucc_tl_ucp_task_t *task = ucc_derived_of(coll_task, ucc_tl_ucp_task_t);
    ucc_tl_ucp_team_t *team = task->team;
    uint32_t           ra_i = task->tag % team->n_ra;
    ptrdiff_t          sbuf = (ptrdiff_t)task->args.src.info_v.buffer;
    ptrdiff_t          rbuf = (ptrdiff_t)task->args.dst.info_v.buffer;
    ucc_memory_type_t  smem = task->args.src.info_v.mem_type;
    ucc_memory_type_t  rmem = task->args.dst.info_v.mem_type;
    ucp_ep_h           ep;
    ucc_status_t       status;
    ucs_status_ptr_t   ucs_status;

    status = ucc_tl_ucp_get_ep(team, team->ra[ra_i].owner, &ep);
    if (ucc_unlikely(UCC_OK != status)) {
        return status;
    }
    ucp_request_param_t param = {
        .op_attr_mask = UCP_OP_ATTR_FIELD_MEMORY_TYPE,
        .memory_type =  UCS_MEMORY_TYPE_HOST,
    };
    ucs_status =
        ucp_put_nbx(ep, &task->tag, sizeof(task->tag),
                    (uintptr_t)(PTR_OFFSET(team->ra[ra_i].seq_num,
                                           team->rank*sizeof(uint32_t))),
                    team->ra[ra_i].rkey, &param);
    if (UCS_PTR_IS_ERR(ucs_status)) {
        tl_error(UCC_TL_TEAM_LIB(team), "failed to put_nbx ra status");
        task->super.super.status = status =
            UCC_ERR_NO_MESSAGE;
    } else if (UCS_OK != ucs_status) {
        task->alltoallv_imbalanced.put_req = (void*)ucs_status;
    }

    /* fprintf(stderr, "rank %d registerd alltoallv imb, seq num %u, ra_i %u\n", */
    /*         team->rank, task->tag, ra_i); */

    int i;
    size_t             rdt_size, sdt_size, data_size, data_displ;
    ucc_rank_t         peer = team->rank;
    /* self */
    rdt_size = ucc_dt_size(task->args.src.info_v.datatype);
    sdt_size = ucc_dt_size(task->args.dst.info_v.datatype);

    data_size  = ucc_coll_args_get_count(&task->args,
                                         task->args.dst.info_v.counts, peer) * rdt_size;
    data_displ = ucc_coll_args_get_displacement(&task->args,
                                                task->args.dst.info_v.displacements,
                                                peer) * rdt_size;
    UCPCHECK_GOTO(ucc_tl_ucp_recv_nz((void *)(rbuf + data_displ),
                                     data_size, rmem, peer, team, task),
                  task, out);
    data_size  = ucc_coll_args_get_count(&task->args,
                                         task->args.src.info_v.counts, peer) * sdt_size;
    data_displ = ucc_coll_args_get_displacement(&task->args,
                                                task->args.src.info_v.displacements,
                                                peer) * sdt_size;
    UCPCHECK_GOTO(ucc_tl_ucp_send_nz((void *)(sbuf + data_displ),
                                     data_size, smem, peer, team, task),
                  task, out);

    /* all other recvs */
    for (i = 0; i < team->size; i++) {
        if (i == team->rank) {
            continue;
        }
        peer = i;
        data_size  = ucc_coll_args_get_count(&task->args,
                                             task->args.dst.info_v.counts, peer) * rdt_size;
        data_displ = ucc_coll_args_get_displacement(&task->args,
                                                    task->args.dst.info_v.displacements,
                                                    peer) * rdt_size;
        UCPCHECK_GOTO(ucc_tl_ucp_recv_nz((void *)(rbuf + data_displ),
                                         data_size, rmem, peer, team, task),
                      task, out);

    }
    ucc_tl_ucp_alltoallv_imbalanced_progress(&task->super);
    if (UCC_INPROGRESS == task->super.super.status) {
        ucc_progress_enqueue(UCC_TL_CORE_CTX(team)->pq, &task->super);
        return UCC_OK;
    }
    return ucc_task_complete(coll_task);
out:
    return status;
}

ucc_status_t
ucc_tl_ucp_alltoallv_imbalanced_init(ucc_base_coll_args_t *coll_args,
                                      ucc_base_team_t      *team,
                                      ucc_coll_task_t     **task_h)
{
    ucc_tl_ucp_team_t *tl_team = ucc_derived_of(team, ucc_tl_ucp_team_t);
    ucc_status_t       status  = UCC_OK;
    ucc_tl_ucp_task_t *task;


    ALLTOALLV_TASK_CHECK(coll_args->args, tl_team);
    task = ucc_tl_ucp_init_task(coll_args, team);
    task->super.post = ucc_tl_ucp_alltoallv_imbalanced_start;
    task->super.finalize = ucc_tl_ucp_alltoallv_imbalanced_finalize;
    task->super.progress = ucc_tl_ucp_alltoallv_imbalanced_progress;
    task->alltoallv_imbalanced.ready =
        ucc_calloc((tl_team->size+1)*2, sizeof(uint32_t), "ready");
    task->alltoallv_imbalanced.put_req = NULL;
    
    if (!task->alltoallv_imbalanced.ready) {
        tl_error(UCC_TL_TEAM_LIB(tl_team),
                 "failed to allocate %zd bytes for ready array",
                 tl_team->size * sizeof(uint32_t));
        ucc_tl_ucp_put_task(task);
        return UCC_ERR_NO_MEMORY;
    }
    *task_h = &task->super;
out:
    return status;
}
