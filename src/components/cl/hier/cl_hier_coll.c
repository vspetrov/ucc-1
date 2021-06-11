/**
 * Copyright (C) Mellanox Technologies Ltd. 2021.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include "cl_hier.h"
#include "utils/ucc_coll_utils.h"
#include "schedule/ucc_schedule_pipelined.h"

ucc_status_t ucc_cl_hier_coll_init(ucc_base_coll_args_t *coll_args,
                                    ucc_base_team_t *team,
                                    ucc_coll_task_t **task)
{
    return UCC_ERR_NOT_IMPLEMENTED;
}

static ucc_status_t ucc_cl_hier_allreduce_hybrid_frag_finalize(ucc_coll_task_t *task)
{
    ucc_status_t status = UCC_OK;
    ucc_schedule_t *schedule = ucc_derived_of(task, ucc_schedule_t);
    status = ucc_schedule_finalize(&schedule->super);
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
    ucc_rank_t       node_size = cl_team->pairs[UCC_HIER_PAIR_NODE_UCP].sbgp->group_size;
    ucc_rank_t       node_rank = cl_team->pairs[UCC_HIER_PAIR_NODE_UCP].sbgp->group_rank;
    int n_frags = schedule_p->super.n_tasks;
    size_t frag_count = args->src.info.count / n_frags;
    size_t left = args->src.info.count % n_frags;
    size_t offset = frag_num * frag_count + left;
    size_t ar_count;
    ucc_coll_task_t *task_rs, *task_ar, *task_ag;

    if (frag_num < left) {
        frag_count++;
        offset -= left - frag_num;
    }
    task_rs = frag->tasks[0];
    task_ar = frag->tasks[1];
    task_ag = frag->tasks[2];

    task_rs->args.src.info.buffer = PTR_OFFSET(args->src.info.buffer, offset * dt_size);
    task_rs->args.dst.info.buffer = PTR_OFFSET(args->dst.info.buffer, offset * dt_size);
    task_rs->args.src.info.count = frag_count;
    task_rs->args.dst.info.count = frag_count;

    ucc_assert(UCC_IS_INPLACE(task_ag->args));
    task_ag->args.dst.info.buffer = PTR_OFFSET(args->dst.info.buffer, offset * dt_size);//only dst since inplace
    task_ag->args.src.info.count = frag_count;
    task_ag->args.dst.info.count = frag_count;

    ucc_assert(UCC_IS_INPLACE(task_ar->args));
    ar_count = frag_count / node_size;
    left = frag_count % node_size;
    offset = node_rank * ar_count + left; //offset from RS dst start
    if (node_rank < left) {
        ar_count++;
        offset -= left - node_rank;
    }
    task_ar->args.src.info.count = ar_count;
    task_ar->args.dst.info.buffer = PTR_OFFSET(task_rs->args.dst.info.buffer,
                                               offset * dt_size);
    return UCC_OK;
}

ucc_status_t ucc_cl_hier_allreduce_hybrid_frag_init(ucc_base_coll_args_t *coll_args,
                                                    ucc_base_team_t *team,
                                                    ucc_schedule_t **frag_p)
{
    ucc_cl_hier_team_t    *cl_team = ucc_derived_of(team, ucc_cl_hier_team_t);
    ucc_base_coll_init_fn_t init;
    ucc_base_team_t        *bteam;
    ucc_status_t            status;
    ucc_coll_task_t *task_rs, *task_ag, *task_ar;
    ucc_base_coll_args_t  args;
    ucc_schedule_t    *schedule = ucc_malloc(sizeof(*schedule), "hier schedule");
    size_t count, left, offset;
    ucc_rank_t node_size = cl_team->pairs[UCC_HIER_PAIR_NODE_UCP].sbgp->group_size;
    ucc_rank_t node_rank = cl_team->pairs[UCC_HIER_PAIR_NODE_UCP].sbgp->group_rank;
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

    /* REDUCE-SCATTER */
    args.args.coll_type = UCC_COLL_TYPE_REDUCE_SCATTER;
    status = ucc_coll_score_map_lookup(cl_team->pairs[UCC_HIER_PAIR_NODE_UCP].score_map,
                                       &args, &init, &bteam);
    ucc_assert(UCC_OK == status);
    status = init(&args, bteam, &task_rs);

    /* ALLREDUCE */
    args.args.coll_type = UCC_COLL_TYPE_ALLREDUCE;
    count = coll_args->args.src.info.count / node_size;
    left = coll_args->args.src.info.count % node_size;
    offset = node_rank * count + left;
    if (node_rank < left) {
        count++;
        offset -= left - node_rank;
    }
    args.args.src.info.count = count;
    args.args.dst.info.buffer = PTR_OFFSET(coll_args->args.dst.info.buffer,
                                           offset * dt_size);
    args.args.mask |= UCC_COLL_ARGS_FIELD_FLAGS;
    args.args.flags |= UCC_COLL_ARGS_FLAG_IN_PLACE;
    status = ucc_coll_score_map_lookup(cl_team->pairs[UCC_HIER_PAIR_NET_UCP].score_map,
                                       &args, &init, &bteam);
    ucc_assert(UCC_OK == status);
    status = init(&args, bteam, &task_ar);

    /* ALLGATHER */
    args.args.coll_type = UCC_COLL_TYPE_ALLGATHER;
    args.args.dst.info.buffer = coll_args->args.dst.info.buffer;
    args.args.src.info.count = coll_args->args.src.info.count;
    status = ucc_coll_score_map_lookup(cl_team->pairs[UCC_HIER_PAIR_NODE_UCP].score_map,
                                       &args, &init, &bteam);
    ucc_assert(UCC_OK == status);
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
    return UCC_OK;
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


ucc_status_t ucc_cl_hier_allreduce_init(ucc_base_coll_args_t *coll_args,
                                    ucc_base_team_t *team,
                                    ucc_coll_task_t **task)
{
    ucc_cl_hier_team_t *cl_team = ucc_derived_of(team, ucc_cl_hier_team_t);
    int n_frags, pipeline_depth;
    ucc_schedule_pipelined_t *schedule_p;
    get_hybrid_n_frags(coll_args, cl_team, &n_frags, &pipeline_depth);

    ucc_schedule_pipelined_init(coll_args, team,
                                ucc_cl_hier_allreduce_hybrid_frag_init,
                                ucc_cl_hier_allreduce_hybrid_setup_frag,
                                pipeline_depth, n_frags, &schedule_p);
    *task = &schedule_p->super.super;
    return UCC_OK;
}
