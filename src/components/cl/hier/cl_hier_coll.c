/**
 * Copyright (C) Mellanox Technologies Ltd. 2021.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include "cl_hier.h"
#include "core/ucc_mc.h"
#include "core/ucc_team.h"
#include "utils/ucc_coll_utils.h"
#include "schedule/ucc_schedule_pipelined.h"

ucc_status_t ucc_cl_hier_coll_init(ucc_base_coll_args_t *coll_args,
                                    ucc_base_team_t *team,
                                    ucc_coll_task_t **task)
{
    return UCC_ERR_NOT_IMPLEMENTED;
}

typedef struct ucc_cl_hier_ar_hybrid_schedule {
    ucc_schedule_pipelined_t super;
    ucc_ee_h ee[3];
} ucc_cl_hier_ar_hybrid_schedule_t;


static ucc_status_t ucc_cl_hier_allreduce_hybrid_frag_finalize(ucc_coll_task_t *task)
{
    ucc_status_t status = UCC_OK;
    ucc_schedule_t *schedule = ucc_derived_of(task, ucc_schedule_t);
    status = ucc_schedule_finalize(&schedule->super);
    ucc_free(schedule);
    return status;
}

static ucc_status_t ucc_cl_hier_ar_hybrid_schedule_finalize(ucc_coll_task_t *task)
{
    ucc_status_t status = UCC_OK;
    ucc_cl_hier_ar_hybrid_schedule_t *schedule = ucc_derived_of(task,
                                                                ucc_cl_hier_ar_hybrid_schedule_t);
    ucc_memory_type_t mt = schedule->super.super.super.args.src.info.mem_type;

    int i;
    for (i = 0; i < 3; i++) {
        ucc_mc_ee_put(schedule->ee[i],(mt == UCC_MEMORY_TYPE_CUDA) ?
                      UCC_EE_CUDA_STREAM : UCC_EE_CPU_THREAD);
    }
    status = ucc_schedule_pipelined_finalize(&schedule->super.super.super);
    ucc_free(schedule);
    return status;
}

ucc_status_t ucc_cl_hier_allreduce_hybrid_setup_frag(ucc_schedule_pipelined_t *schedule_p,
                                                     ucc_schedule_t *frag, int frag_num)
{
    ucc_coll_args_t *args = &schedule_p->super.super.args;
    ucc_cl_hier_team_t    *cl_team = ucc_derived_of(schedule_p->super.super.team, ucc_cl_hier_team_t);
    ucc_datatype_t   dt        = args->src.info.datatype;
    size_t           dt_size   = ucc_dt_size(dt);
    ucc_rank_t       node_size = cl_team->sbgps[UCC_HIER_SBGP_NODE].sbgp->group_size;
    ucc_rank_t       node_rank = cl_team->sbgps[UCC_HIER_SBGP_NODE].sbgp->group_rank;
    int n_frags = schedule_p->super.n_tasks;
    size_t frag_count = args->src.info.count / n_frags;
    size_t left = args->src.info.count % n_frags;
    size_t frag_offset = frag_num * frag_count + left;
    size_t ar_count, offset;
    ucc_coll_task_t *task_rs, *task_ar, *task_ag;

    if (frag_num < left) {
        frag_count++;
        frag_offset -= left - frag_num;
    }

    ar_count = frag_count / node_size;
    left = frag_count % node_size;
    offset = node_rank * ar_count + left; //offset from RS dst start
    if (node_rank < left) {
        ar_count++;
        offset -= left - node_rank;
    }

    task_rs = frag->tasks[0];
    task_ar = frag->tasks[1];
    task_ag = frag->tasks[2];

    task_rs->args.src.info.buffer = PTR_OFFSET(args->src.info.buffer, frag_offset * dt_size);
    task_rs->args.dst.info.buffer = PTR_OFFSET(args->dst.info.buffer, (frag_offset + offset) * dt_size);
    task_rs->args.src.info.count = frag_count;
    task_rs->args.dst.info.count = frag_count;

    ucc_assert(UCC_IS_INPLACE(task_ag->args));
    task_ag->args.dst.info.buffer = PTR_OFFSET(args->dst.info.buffer, frag_offset * dt_size);//only dst since inplace
    task_ag->args.src.info.count = frag_count;
    task_ag->args.dst.info.count = frag_count;

    ucc_assert(UCC_IS_INPLACE(task_ar->args));
    task_ar->args.src.info.count = ar_count;
    task_ar->args.dst.info.buffer = task_rs->args.dst.info.buffer;

    return UCC_OK;
}

ucc_status_t ucc_cl_hier_allreduce_hybrid_frag_init(ucc_base_coll_args_t *coll_args,
                                                    ucc_schedule_pipelined_t *sp,
                                                    ucc_base_team_t *team,
                                                    ucc_schedule_t **frag_p)
{
    ucc_cl_hier_team_t    *cl_team = ucc_derived_of(team, ucc_cl_hier_team_t);
    ucc_base_coll_init_fn_t init;
    ucc_base_team_t        *bteam;
    ucc_status_t            status = UCC_OK;
    ucc_coll_task_t *task_rs, *task_ag, *task_ar;
    ucc_base_coll_args_t  args;
    ucc_schedule_t    *schedule = ucc_malloc(sizeof(*schedule), "hier schedule");
    ucc_cl_hier_ar_hybrid_schedule_t *sched_hybrid =
        ucc_derived_of(sp, ucc_cl_hier_ar_hybrid_schedule_t);
    size_t count, left, offset;
    ucc_rank_t node_size = cl_team->sbgps[UCC_HIER_SBGP_NODE].sbgp->group_size;
    ucc_rank_t node_rank = cl_team->sbgps[UCC_HIER_SBGP_NODE].sbgp->group_rank;
    size_t dt_size = ucc_dt_size(coll_args->args.src.info.datatype);

    /* void *src = UCC_IS_INPLACE(coll_args->args) ? */
        /* coll_args->args.dst.info.buffer : coll_args->args.src.info.buffer; */

    /* printf("ALLREDUCE: sbuf %p, rbuf %p, count %zd\n", */
    /*        coll_args->args.src.info.buffer, coll_args->args.dst.info.buffer, */
    /*        coll_args->args.src.info.count); */
    ucc_assert(schedule);

    memcpy(&args, coll_args, sizeof(args));
    ucc_schedule_init(schedule, &args.args, team, 0);
    schedule->super.team = team; //TODO move to schedule init

    count = coll_args->args.src.info.count / node_size;
    left = coll_args->args.src.info.count % node_size;
    offset = node_rank * count + left;
    if (node_rank < left) {
        count++;
        offset -= left - node_rank;
    }

    /* REDUCE-SCATTER */
    args.args.coll_type = UCC_COLL_TYPE_REDUCE_SCATTER;
    args.args.dst.info.buffer = PTR_OFFSET(args.args.dst.info.buffer, offset * dt_size);
    status = ucc_coll_score_map_lookup(cl_team->sbgps[UCC_HIER_SBGP_NODE].score_map,
                                       &args, &init, &bteam);
    ucc_assert(UCC_OK == status);
    args.ee = sched_hybrid->ee[0];
    status = init(&args, bteam, &task_rs);


    /* ALLREDUCE */
    args.args.coll_type = UCC_COLL_TYPE_ALLREDUCE;
    args.args.src.info.count = count;
    args.args.mask |= UCC_COLL_ARGS_FIELD_FLAGS;
    args.args.flags |= UCC_COLL_ARGS_FLAG_IN_PLACE;
    status = ucc_coll_score_map_lookup(cl_team->sbgps[UCC_HIER_SBGP_NET].score_map,
                                       &args, &init, &bteam);
    ucc_assert(UCC_OK == status);
    args.ee = sched_hybrid->ee[1];
    status = init(&args, bteam, &task_ar);

    /* ALLGATHER */
    args.args.coll_type = UCC_COLL_TYPE_ALLGATHER;
    args.args.dst.info.buffer = coll_args->args.dst.info.buffer;
    args.args.dst.info.datatype = coll_args->args.src.info.datatype;
    args.args.dst.info.count = coll_args->args.src.info.count;
    status = ucc_coll_score_map_lookup(cl_team->sbgps[UCC_HIER_SBGP_NODE2].score_map,
                                       &args, &init, &bteam);
    ucc_assert(UCC_OK == status);
    args.ee = sched_hybrid->ee[2];
    status = init(&args, bteam, &task_ag);

    task_rs->n_deps = 1;
    ucc_schedule_add_task(schedule, task_rs);
    ucc_event_manager_subscribe(&schedule->super.em, UCC_EVENT_SCHEDULE_STARTED,
                                task_rs, ucc_dependency_handler);

    task_ar->n_deps = 1;
    ucc_schedule_add_task(schedule, task_ar);
    ucc_event_manager_subscribe(&task_rs->em, UCC_EVENT_COMPLETED,
                                task_ar, ucc_dependency_handler);

    task_ag->n_deps = 1;
    ucc_schedule_add_task(schedule, task_ag);
    ucc_event_manager_subscribe(&task_ar->em, UCC_EVENT_COMPLETED,
                                task_ag, ucc_dependency_handler);

    schedule->super.post     = ucc_schedule_post;
    schedule->super.progress = NULL;
    schedule->super.finalize = ucc_cl_hier_allreduce_hybrid_frag_finalize;

    *frag_p = schedule;
    return status;
}

static inline void get_hybrid_n_frags(ucc_base_coll_args_t *coll_args,
                                      ucc_cl_hier_team_t      *team,
                                      int *n_frags, int *pipeline_depth)
{
//    ucc_memory_type_t mt = coll_args->args.src.info.mem_type;
    //TODO make selection mem_type - specific
    ucc_cl_hier_lib_config_t *cfg = &UCC_CL_HIER_TEAM_LIB(team)->cfg;
    size_t msgsize = coll_args->args.src.info.count *
        ucc_dt_size(coll_args->args.src.info.datatype);
    *n_frags = 1;
    if (msgsize > cfg->allreduce_hybrid_frag_thresh) {
        int min_num_frags = msgsize/cfg->allreduce_hybrid_frag_size;
        *n_frags = ucc_max(min_num_frags,
                          cfg->allreduce_hybrid_n_frags);
    }
    *pipeline_depth = ucc_min(*n_frags, cfg->allreduce_hybrid_pipeline_depth);
    /* printf("pd %d, n_fragas %d\n", *pipeline_depth, *n_frags); */
}

static ucc_status_t ucc_cl_hier_hybrid_allreduce_post(ucc_coll_task_t *task)
{
    ucc_schedule_pipelined_t *schedule = ucc_derived_of(task,
                                                        ucc_schedule_pipelined_t);
    cl_info(task->team->context->lib,
            "posting hybrid ar, sbuf %p, rbuf %p, count %zd, dt %s, op %s, "
             "inplace %d, pdepth %d, frags_total %d",
             task->args.src.info.buffer, task->args.dst.info.buffer,
             task->args.src.info.count,
             ucc_datatype_str(task->args.src.info.datatype),
             ucc_reduction_op_str(task->args.reduce.predefined_op),
             UCC_IS_INPLACE(task->args), schedule->n_frags,
             schedule->super.n_tasks);
    return ucc_schedule_pipelined_post(task);
}

ucc_status_t ucc_cl_hier_allreduce_init(ucc_base_coll_args_t *coll_args,
                                    ucc_base_team_t *team,
                                    ucc_coll_task_t **task)
{
    ucc_cl_hier_team_t *cl_team = ucc_derived_of(team, ucc_cl_hier_team_t);
    int n_frags, pipeline_depth;
    ucc_memory_type_t mt = coll_args->args.src.info.mem_type;
    ucc_cl_hier_lib_config_t *cfg = &UCC_CL_HIER_TEAM_LIB(cl_team)->cfg;
    ucc_cl_hier_ar_hybrid_schedule_t *schedule =
        ucc_malloc(sizeof(*schedule), "cl_hier_ar_hybrid_sched");
    if (!schedule) {
        cl_error(team->context->lib, "failed to allocate %zd bytes for hybrid schedule",
                 sizeof(*schedule));
        return UCC_ERR_NO_MEMORY;
    }
    int i;
    for (i = 0; i < 3; i++) {
        ucc_mc_ee_get(&schedule->ee[i], (mt == UCC_MEMORY_TYPE_CUDA) ?
                      UCC_EE_CUDA_STREAM : UCC_EE_CPU_THREAD);
    }


    get_hybrid_n_frags(coll_args, cl_team, &n_frags, &pipeline_depth);
    ucc_schedule_pipelined_init(coll_args, team,
                                ucc_cl_hier_allreduce_hybrid_frag_init,
                                ucc_cl_hier_allreduce_hybrid_setup_frag,
                                pipeline_depth, n_frags, cfg->allreduce_hybrid_seq,
                                &schedule->super);

    schedule->super.super.super.post = ucc_cl_hier_hybrid_allreduce_post;
    schedule->super.super.super.finalize = ucc_cl_hier_ar_hybrid_schedule_finalize;
    *task = &schedule->super.super.super;
    return UCC_OK;
}
