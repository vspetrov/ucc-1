/**
 * Copyright (C) Mellanox Technologies Ltd. 2020-2021.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include "cl_hier.h"
#include "utils/ucc_malloc.h"
#include "core/ucc_team.h"

ucc_status_t ucc_cl_hier_oob_allgather(void *src_buf, void *recv_buf, size_t size,
                                       void *allgather_info,  void **request)
{
    ucc_sbgp_t *sbgp = (ucc_sbgp_t*)allgather_info;
    ucc_team_t *team = sbgp->team;
    ucc_tl_iface_t  *tl_iface = UCC_TL_TEAM_IFACE(team->service_team);
    ucc_tl_team_subset_t subset = {
        .map.type   = UCC_EP_MAP_ARRAY,
        .map.array.map = sbgp->rank_map,
        .map.array.elem_size = sizeof(ucc_rank_t),
        .map.ep_num = sbgp->group_size,
        .myrank     = sbgp->group_rank,
    };
    ucc_status_t status;
    ucc_coll_task_t *task;
    status = tl_iface->scoll.allgather(
        &team->service_team->super, src_buf, recv_buf, size,
        subset, &task);
    if (status < 0) {
        ucc_error("failed to start service allgather in cl hier pair creation");
        return status;
    }
    *request = (void*)task;
    return status;
}

ucc_status_t ucc_cl_hier_oob_req_test(void *request)
{
    ucc_coll_task_t *task = (ucc_coll_task_t*)request;
    return task->super.status;
}

ucc_status_t ucc_cl_hier_oob_req_free(void *request)
{
    ucc_coll_task_t *task = (ucc_coll_task_t*)request;
    return task->finalize(task);
}

UCC_CLASS_INIT_FUNC(ucc_cl_hier_team_t, ucc_base_context_t *cl_context,
                    const ucc_base_team_params_t *params)
{
    ucc_cl_hier_context_t *ctx =
        ucc_derived_of(cl_context, ucc_cl_hier_context_t);
    int                     i;
    ucc_status_t            status;

    UCC_CLASS_CALL_SUPER_INIT(ucc_cl_team_t, &ctx->super, params->team);

    self->pairs[UCC_HIER_PAIR_NODE_UCP].state = UCC_HIER_PAIR_ENABLED;
    self->pairs[UCC_HIER_PAIR_NODE_UCP].sbgp =
        ucc_team_topo_get_sbgp(params->team->topo, UCC_SBGP_NODE);
    ucc_tl_context_get(ctx->super.super.ucc_context,
                       "ucp", &self->pairs[UCC_HIER_PAIR_NODE_UCP].tl_ctx);

    self->pairs[UCC_HIER_PAIR_NET_UCP].state = UCC_HIER_PAIR_ENABLED;
    self->pairs[UCC_HIER_PAIR_NET_UCP].sbgp =
        ucc_team_topo_get_sbgp(params->team->topo, UCC_SBGP_NET);
    ucc_tl_context_get(ctx->super.super.ucc_context,
                       "ucp", &self->pairs[UCC_HIER_PAIR_NET_UCP].tl_ctx);
    /* ucc_sbgp_print(self->pairs[UCC_HIER_PAIR_NODE_UCP].sbgp); */
    /* ucc_sbgp_print(self->pairs[UCC_HIER_PAIR_NET_UCP].sbgp); */
    int n_teams = 0;
    for (i = 0; i < UCC_HIER_PAIR_LAST; i++) {
        if (self->pairs[i].state == UCC_HIER_PAIR_ENABLED) {
            n_teams++;
        }
    }

    self->n_tl_teams = 0;
    status           = ucc_team_multiple_req_alloc(&self->team_create_req,
                                                   n_teams);
    if (UCC_OK != status) {
        cl_error(cl_context->lib, "failed to allocate team req multiple");
        goto err;
    }
    int j = 0;

    for (i = 0; i < UCC_HIER_PAIR_LAST; i++) {
        if (self->pairs[i].state == UCC_HIER_PAIR_ENABLED) {
            /* memcpy(&self->team_create_req->descs[j].param, params, */
            /* sizeof(ucc_base_team_params_t)); */

            self->team_create_req->descs[j].param.team =
                params->team;
            self->team_create_req->descs[j].param.rank =
                self->pairs[i].sbgp->group_rank;
            self->team_create_req->descs[j].param.params.team_size =
                self->pairs[i].sbgp->group_size;
            self->team_create_req->descs[j].param.params.mask =
                UCC_TEAM_PARAM_FIELD_EP_RANGE |
                UCC_TEAM_PARAM_FIELD_EP |
                UCC_TEAM_PARAM_FIELD_TEAM_SIZE |
                UCC_TEAM_PARAM_FIELD_OOB;
            self->team_create_req->descs[j].param.params.ep =
                (uint64_t)self->pairs[i].sbgp->group_rank;
            self->team_create_req->descs[j].param.params.ep_range =
                UCC_COLLECTIVE_EP_RANGE_CONTIG;
            self->team_create_req->descs[j].ctx            =
                self->pairs[i].tl_ctx;
            self->team_create_req->descs[j].param.scope    = UCC_CL_HIER;
            self->team_create_req->descs[j].param.id =
                params->id;
            self->team_create_req->descs[j].param.scope_id =
                self->pairs[i].sbgp->type;
            self->team_create_req->descs[j].param.map.type = UCC_EP_MAP_ARRAY;
            self->team_create_req->descs[j].param.map.array.map =
                self->pairs[i].sbgp->rank_map;
            self->team_create_req->descs[j].param.map.array.elem_size =
                sizeof(ucc_rank_t);
            self->team_create_req->descs[j].param.map.ep_num =
                self->pairs[i].sbgp->group_size;
            self->team_create_req->descs[j].param.params.oob.allgather = ucc_cl_hier_oob_allgather;
            self->team_create_req->descs[j].param.params.oob.req_test = ucc_cl_hier_oob_req_test;
            self->team_create_req->descs[j].param.params.oob.req_free = ucc_cl_hier_oob_req_free;
            self->team_create_req->descs[j].param.params.oob.participants = self->pairs[i].sbgp->group_size;
            self->team_create_req->descs[j].param.params.oob.coll_info = (void*)self->pairs[i].sbgp;
            j++;
        }
    }
    self->team_create_req->n_teams = n_teams;

    status = ucc_tl_team_create_multiple(self->team_create_req);
    if (status < 0) {
        cl_error(cl_context->lib, "failed to post tl team create (%d)",
                 status);
        goto err;
    }
    cl_info(cl_context->lib, "posted cl team: %p", self);
    return UCC_OK;
err:
    return status;
}

UCC_CLASS_CLEANUP_FUNC(ucc_cl_hier_team_t)
{
    cl_info(self->super.super.context->lib, "finalizing cl team: %p", self);
}

UCC_CLASS_DEFINE_DELETE_FUNC(ucc_cl_hier_team_t, ucc_base_team_t);
UCC_CLASS_DEFINE(ucc_cl_hier_team_t, ucc_cl_team_t);

ucc_status_t ucc_cl_hier_team_destroy(ucc_base_team_t *cl_team)
{
    ucc_cl_hier_team_t    *team    = ucc_derived_of(cl_team, ucc_cl_hier_team_t);
    ucc_cl_hier_context_t *ctx     = UCC_CL_HIER_TEAM_CTX(team);
    ucc_status_t            status  = UCC_OK;
    int                     i, j;


    if (NULL == team->team_create_req) {
        status = ucc_team_multiple_req_alloc(&team->team_create_req,
                                             team->n_tl_teams);
        if (UCC_OK != status) {
            cl_error(ctx->super.super.lib, "failed to allocate team req multiple");
            return status;
        }
        team->team_create_req->n_teams       = team->n_tl_teams;
        j = 0;
        for (i = 0; i < UCC_HIER_PAIR_LAST; i++) {
            if (team->pairs[i].state == UCC_HIER_PAIR_ENABLED) {
                ucc_coll_score_free_map(team->pairs[i].score_map);
                ucc_tl_context_put(team->pairs[i].tl_ctx);
                team->team_create_req->descs[j].team =
                    team->pairs[i].tl_team;
                j++;
            }
        }
    }
    ucc_assert(j == team->n_tl_teams);
    status = ucc_tl_team_destroy_multiple(team->team_create_req);
    if (UCC_INPROGRESS == status) {
        return status;
    }
    for (i = 0; i < team->n_tl_teams; i++) {
        if (team->team_create_req->descs[i].status != UCC_OK) {
            cl_error(ctx->super.super.lib, "tl team destroy failed (%d)",
                     status);
            status = team->team_create_req->descs[i].status;
        }
    }
    ucc_team_multiple_req_free(team->team_create_req);
    UCC_CLASS_DELETE_FUNC_NAME(ucc_cl_hier_team_t)(cl_team);
    return status;
}

ucc_status_t ucc_cl_hier_team_create_test(ucc_base_team_t *cl_team)
{
    ucc_cl_hier_team_t    *team = ucc_derived_of(cl_team, ucc_cl_hier_team_t);
    ucc_cl_hier_context_t *ctx  = UCC_CL_HIER_TEAM_CTX(team);
    ucc_status_t            status;
    int                     i, j;
    ucc_coll_score_t *score;
    status = ucc_tl_team_create_multiple(team->team_create_req);

    if (status == UCC_OK) {
        team->n_tl_teams = 0;
        j = 0;
        for (i = 0; i < UCC_HIER_PAIR_LAST; i++) {
            if (team->pairs[i].state == UCC_HIER_PAIR_ENABLED) {
                if (team->team_create_req->descs[i].status == UCC_OK) {
                    team->pairs[i].tl_team =
                        team->team_create_req->descs[i].team;
                    status =
                        UCC_TL_TEAM_IFACE(team->pairs[i].tl_team)
                        ->team.get_scores(&team->pairs[i].tl_team->super,
                                          &score);
                    if (UCC_OK != status) {
                        cl_error(ctx->super.super.lib, "failed to get tl %s scores",
                                 UCC_TL_TEAM_IFACE(team->pairs[i].tl_team)->super.name);
                        team->pairs[i].state = UCC_HIER_PAIR_DISABLED;
                        continue;
                        //goto cleanup ?
                    }
                    status = ucc_coll_score_build_map(score, &team->pairs[i].score_map);
                    if (UCC_OK != status) {
                        cl_error(ctx->super.super.lib, "failed to build score map");
                        continue;
                        //goto cleanup ?
                    }
                    team->n_tl_teams++;
                    cl_info(ctx->super.super.lib, "initialized tl %s team",
                            UCC_TL_CTX_IFACE(team->team_create_req->descs[i].ctx)->
                            super.name);
                } else {
                    cl_error(ctx->super.super.lib, "failed to create tl %s team",
                            UCC_TL_CTX_IFACE(team->team_create_req->descs[i].ctx)->
                            super.name);
                    team->pairs[i].state = UCC_HIER_PAIR_DISABLED;
                }
                j++;
            }
        }
        ucc_team_multiple_req_free(team->team_create_req);
        team->team_create_req = NULL;

        /* if (0 == team->n_tl_teams) { */
        /*     cl_error(ctx->super.super.lib, "no tl teams were created"); */
        /*     return UCC_ERR_NOT_FOUND; */
        /* } */
    }
    return status;
}

ucc_status_t ucc_cl_hier_team_get_scores(ucc_base_team_t *cl_team,
                                          ucc_coll_score_t **score_p)
{
    ucc_cl_hier_team_t *team = ucc_derived_of(cl_team, ucc_cl_hier_team_t);
    ucc_base_lib_t      *lib  = UCC_CL_TEAM_LIB(team);
    ucc_coll_score_t  *score;
    ucc_status_t       status;

    status = ucc_coll_score_alloc(&score);
    if (UCC_OK != status) {
        cl_error(lib, "faild to alloc score_t");
        return status;
    }
    status = ucc_coll_score_add_range(score, UCC_COLL_TYPE_ALLREDUCE,
                                      UCC_MEMORY_TYPE_HOST, 65536, UCC_MSG_MAX,
                                      UCC_CL_HIER_DEFAULT_SCORE, ucc_cl_hier_allreduce_init,
                                      cl_team);
    if (UCC_OK != status) {
        cl_error(lib, "faild to add range to score_t");
        return status;
    }
    if (strlen(lib->score_str) > 0) {
        status = ucc_coll_score_update_from_str(
            lib->score_str, score, cl_team->team->size,
            NULL, cl_team, UCC_CL_HIER_DEFAULT_SCORE, NULL);

        /* If INVALID_PARAM - User provided incorrect input - try to proceed */
        if ((status < 0) && (status != UCC_ERR_INVALID_PARAM) &&
            (status != UCC_ERR_NOT_SUPPORTED)) {
            goto err;
        }
    }
    *score_p = score;
    return UCC_OK;
err:
    ucc_coll_score_free(score);
    *score_p = NULL;
    return status;
}
