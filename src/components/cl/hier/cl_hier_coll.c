/**
 * Copyright (C) Mellanox Technologies Ltd. 2021.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include "cl_hier.h"
#include "utils/ucc_coll_utils.h"

ucc_status_t ucc_cl_hier_coll_init(ucc_base_coll_args_t *coll_args,
                                    ucc_base_team_t *team,
                                    ucc_coll_task_t **task)
{
    return UCC_ERR_NOT_IMPLEMENTED;
}

static ucc_status_t ucc_cl_hier_schedule_finalize(ucc_coll_task_t *task)
{
    ucc_schedule_t *schedule = ucc_derived_of(task, ucc_schedule_t);
    ucc_status_t status;
    status = ucc_schedule_finalize(task);
    ucc_free(schedule);
    return status;
}

ucc_status_t ucc_cl_hier_allreduce_init(ucc_base_coll_args_t *coll_args,
                                    ucc_base_team_t *team,
                                    ucc_coll_task_t **task)
{
    ucc_cl_hier_team_t    *cl_team = ucc_derived_of(team, ucc_cl_hier_team_t);
    ucc_base_coll_init_fn_t init;
    ucc_base_team_t        *bteam;
    ucc_status_t            status;
    ucc_coll_task_t *task_rs, *task_ag, *task_ar;
    ucc_base_coll_args_t  args;
    ucc_schedule_t      *schedule = ucc_malloc(sizeof(*schedule), "hier schedule");
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

    ucc_schedule_init(schedule, UCC_CL_CORE_CTX(cl_team));
    memcpy(&args, coll_args, sizeof(args));


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

//    task_rs->flags = UCC_COLL_TASK_FLAG_INTERNAL; // <- useless need to cleanup manually


    ucc_schedule_add_task(schedule, task_rs);
    ucc_event_manager_subscribe(&schedule->super.em, UCC_EVENT_SCHEDULE_STARTED,
                                task_rs, ucc_task_start_handler);


    ucc_schedule_add_task(schedule, task_ar);
    ucc_event_manager_subscribe(&task_rs->em, UCC_EVENT_COMPLETED,
                                task_ar, ucc_task_start_handler);

    ucc_schedule_add_task(schedule, task_ag);
    ucc_event_manager_subscribe(&task_ar->em, UCC_EVENT_COMPLETED,
                                task_ag, ucc_task_start_handler);

    schedule->super.post     = ucc_schedule_post;
    schedule->super.progress = NULL;
    schedule->super.finalize = ucc_cl_hier_schedule_finalize;

    *task = &schedule->super;
    return UCC_OK;
}
