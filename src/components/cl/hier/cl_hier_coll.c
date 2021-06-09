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
    ucc_free(schedule);
    return UCC_OK;
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
    ucc_schedule_t      *schedule = ucc_malloc(sizeof(*schedule), "hier schedule");
    fprintf(stderr,"[%d][%s] -- [%d]\n",getpid(),__FUNCTION__,__LINE__);
    ucc_assert(schedule);

    ucc_schedule_init(schedule, UCC_CL_CORE_CTX(cl_team));
    status = ucc_coll_score_map_lookup(cl_team->pairs[UCC_HIER_PAIR_NODE_UCP].score_map,
                                       coll_args, &init, &bteam);
    ucc_assert(UCC_OK == status);
    status = init(coll_args, bteam, &task_rs);

    status = ucc_coll_score_map_lookup(cl_team->pairs[UCC_HIER_PAIR_NET_UCP].score_map,
                                       coll_args, &init, &bteam);
    ucc_assert(UCC_OK == status);
    status = init(coll_args, bteam, &task_ar);

    status = ucc_coll_score_map_lookup(cl_team->pairs[UCC_HIER_PAIR_NODE_UCP].score_map,
                                       coll_args, &init, &bteam);
    ucc_assert(UCC_OK == status);
    status = init(coll_args, bteam, &task_ag);

//    task_rs->flags = UCC_COLL_TASK_FLAG_INTERNAL; // <- useless need to cleanup manually
    ucc_schedule_add_task(schedule, task_rs);
    ucc_event_manager_subscribe(&schedule->super.em, UCC_EVENT_SCHEDULE_STARTED,
                                task_rs);
    task_rs->handlers[UCC_EVENT_SCHEDULE_STARTED] = ucc_task_start_handler;


    ucc_schedule_add_task(schedule, task_ar);
    ucc_event_manager_subscribe(&task_rs->em, UCC_EVENT_COMPLETED,
                                task_ar);
    task_ar->handlers[UCC_EVENT_COMPLETED] = ucc_task_start_handler;


    ucc_schedule_add_task(schedule, task_ag);
    ucc_event_manager_subscribe(&task_ar->em, UCC_EVENT_COMPLETED,
                                task_ag);
    task_ag->handlers[UCC_EVENT_COMPLETED] = ucc_task_start_handler;

    schedule->super.post     = ucc_schedule_post;
    schedule->super.progress = NULL;
    schedule->super.finalize = ucc_cl_hier_schedule_finalize;

    *task = &schedule->super;
    return UCC_OK;
}
