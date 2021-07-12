/**
 * Copyright (C) Mellanox Technologies Ltd. 2020-2021.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include "tl_ucp.h"
#include "tl_ucp_ep.h"
#include "tl_ucp_coll.h"
#include "tl_ucp_sendrecv.h"
#include "utils/ucc_malloc.h"
#include "coll_score/ucc_coll_score.h"

UCC_CLASS_INIT_FUNC(ucc_tl_ucp_team_t, ucc_base_context_t *tl_context,
                    const ucc_base_team_params_t *params)
{
    ucc_tl_ucp_context_t *ctx =
        ucc_derived_of(tl_context, ucc_tl_ucp_context_t);
    UCC_CLASS_CALL_SUPER_INIT(ucc_tl_team_t, &ctx->super, params->team);
    /* TODO: init based on ctx settings and on params: need to check
             if all the necessary ranks mappings are provided */
    self->preconnect_task    = NULL;
    self->size               = params->params.oob.participants;
    self->scope              = params->scope;
    self->scope_id           = params->scope_id;
    self->rank               = params->rank;
    self->id                 = params->id;
    self->seq_num            = 0;
    self->status             = UCC_INPROGRESS;
    self->n_ra               = ucc_min(N_READY_ARRAYS, self->size);
    if (self->rank < self->n_ra) {
        ready_array_t *ra = &self->ra[self->rank];
        ra->owner = self->rank;
        ra->seq_num = ucc_calloc(sizeof(*ra->seq_num), (self->size + 1), "seq_n");
        if (!ra->seq_num) {
            tl_error(tl_context->lib, "failed to allocated %zd bytes for a2av_signal",
                     sizeof(*ra->seq_num) * self->size);
            return UCC_ERR_NO_MEMORY;
        }
        ra->seq_num[self->size] = 0xdeadbeef;
        ucp_mem_map_params_t map_p = {
            .field_mask = UCP_MEM_MAP_PARAM_FIELD_ADDRESS |
            UCP_MEM_MAP_PARAM_FIELD_LENGTH,
            .address = ra->seq_num,
            .length = sizeof(*ra->seq_num) * (self->size+1),

        };

        ucs_status_t status = ucp_mem_map(ctx->ucp_context, &map_p, &ra->memh);
        if (UCS_OK != status) {
            tl_error(tl_context->lib, "failed to map a2a_signal");
            return ucs_status_to_ucc_status(status);
        }
        size_t rkey_size;
        void *rkey_buffer;
        status = ucp_rkey_pack(ctx->ucp_context, ra->memh, &rkey_buffer, &rkey_size);
        if (status != UCS_OK) {
            tl_error(tl_context->lib, "failed to pack ra rkey");
            return ucs_status_to_ucc_status(status);
        }
        if (rkey_size > MAX_RKEY_SIZE) {
            tl_error(tl_context->lib, "rkey size too large %zd",
                     rkey_size);
            return UCC_ERR_NO_MESSAGE;
        }
        memcpy(ra->packed_key, rkey_buffer, rkey_size);
        ucp_rkey_buffer_release(rkey_buffer);
    }
    self->state = UCC_TL_UCP_TEAM_STATE_PRECONNECT;
    tl_info(tl_context->lib, "posted tl team: %p", self);
    return UCC_OK;
}

UCC_CLASS_CLEANUP_FUNC(ucc_tl_ucp_team_t)
{
    int i;
    for (i = 0; i < self->n_ra; i++) {
        ucp_rkey_destroy(self->ra[i].rkey);
    }
    if (self->rank < self->n_ra) {
        ready_array_t *ra = &self->ra[self->rank];
        ucp_mem_unmap(UCC_TL_UCP_TEAM_CTX(self)->ucp_context, ra->memh);
        ucc_free(ra->seq_num);
    }
    tl_info(self->super.super.context->lib, "finalizing tl team: %p", self);
}

UCC_CLASS_DEFINE_DELETE_FUNC(ucc_tl_ucp_team_t, ucc_base_team_t);
UCC_CLASS_DEFINE(ucc_tl_ucp_team_t, ucc_tl_team_t);

ucc_status_t ucc_tl_ucp_team_destroy(ucc_base_team_t *tl_team)
{

    UCC_CLASS_DELETE_FUNC_NAME(ucc_tl_ucp_team_t)(tl_team);
    return UCC_OK;
}

static ucc_status_t ucc_tl_ucp_team_preconnect(ucc_tl_ucp_team_t *team)
{
    ucc_rank_t src, dst;
    ucc_status_t status;
    int i;
    if (!team->preconnect_task) {
        team->preconnect_task = ucc_tl_ucp_get_task(team);
        team->preconnect_task->tag = 0;
    }
    if (UCC_INPROGRESS == ucc_tl_ucp_test(team->preconnect_task)) {
        return UCC_INPROGRESS;
    }
    for (i = team->preconnect_task->send_posted; i < team->size; i++) {
        src = (team->rank - i + team->size) % team->size;
        dst = (team->rank + i) % team->size;
        status = ucc_tl_ucp_send_nb(NULL, 0, UCC_MEMORY_TYPE_UNKNOWN, src, team,
                                    team->preconnect_task);
        if (UCC_OK != status) {
            return status;
        }
        status = ucc_tl_ucp_recv_nb(NULL, 0, UCC_MEMORY_TYPE_UNKNOWN, dst, team,
                                    team->preconnect_task);
        if (UCC_OK != status) {
            return status;
        }
        if (UCC_INPROGRESS == ucc_tl_ucp_test(team->preconnect_task)) {
            return UCC_INPROGRESS;
        }
    }
    tl_debug(UCC_TL_TEAM_LIB(team), "preconnected tl team: %p, num_eps %d",
             team, team->size);
    ucc_tl_ucp_put_task(team->preconnect_task);
    team->preconnect_task = NULL;
    return UCC_OK;
}

ucc_status_t ucc_tl_ucp_team_create_test(ucc_base_team_t *tl_team)
{
    ucc_tl_ucp_team_t    *team = ucc_derived_of(tl_team, ucc_tl_ucp_team_t);
    ucc_tl_ucp_context_t *ctx  = UCC_TL_UCP_TEAM_CTX(team);
    int                   i;
    ucc_status_t          status;
    if (team->status == UCC_OK) {
        return UCC_OK;
    }
    if (team->size <= ctx->cfg.preconnect && team->state == UCC_TL_UCP_TEAM_STATE_PRECONNECT) {
        status = ucc_tl_ucp_team_preconnect(team);
        if (UCC_INPROGRESS == status) {
            return UCC_INPROGRESS;
        } else if (UCC_OK != status) {
            goto err_preconnect;
        }
    }

    if (team->state == UCC_TL_UCP_TEAM_STATE_PRECONNECT) {
        team->state = UCC_TL_UCP_TEAM_STATE_RA_EXCHANGE;
        ready_array_t *ra = ucc_malloc(sizeof(*ra) * team->size);
        if (!ra) {
            tl_error(tl_team->context->lib,
                     "failed to allocate %zd bytes for global ra",
                     sizeof(*ra) * team->size);
            return UCC_ERR_NO_MEMORY;
        }
        if (team->rank < team->n_ra) {
            memcpy(&ra[team->rank], &team->ra[team->rank], sizeof(*ra));
        }
        ucc_base_coll_args_t bargs = {
            .args = {
                .mask  = UCC_COLL_ARGS_FIELD_FLAGS,
                .flags = UCC_COLL_ARGS_FLAG_IN_PLACE,
                .coll_type = UCC_COLL_TYPE_ALLGATHER,
                .dst.info.buffer = ra,
                .dst.info.count  = sizeof(*ra), //WILL change due to api change
                .dst.info.datatype = UCC_DT_UINT8,
                .dst.info.mem_type = UCC_MEMORY_TYPE_HOST
            },
            .team = tl_team->team
        };
        status = ucc_tl_ucp_coll_init(&bargs, tl_team,
                                      (ucc_coll_task_t**)&team->preconnect_task);
        if (UCC_OK != status) {
            tl_error(tl_team->context->lib, "failed to init ra allgather");
            ucc_free(ra);
            return status;
        }
        status = team->preconnect_task->super.post(&team->preconnect_task->super);
        if (UCC_OK != status) {
            tl_error(tl_team->context->lib, "failed to post ra allgather");
            ucc_free(ra);
            return status;
        }
    }
    if (team->preconnect_task &&
        team->preconnect_task->super.super.status != UCC_OK) {
        ucc_context_progress(UCC_TL_CORE_CTX(team));
        if (team->preconnect_task->super.super.status != UCC_OK) {
            if (team->preconnect_task->super.super.status < 0) {
                tl_error(tl_team->context->lib, "failure during ra allgather");
            }
            return team->preconnect_task->super.super.status;
        }
    }
    ready_array_t *ra = team->preconnect_task->args.dst.info.buffer;
    team->preconnect_task->super.finalize(&team->preconnect_task->super);
    team->preconnect_task = NULL;
    for (i = 0; i < team->n_ra; i++) {
        ucp_ep_h ep;
        status = ucc_tl_ucp_get_ep(team, i, &ep);
        if (ucc_unlikely(UCC_OK != status)) {
            return status;
        }
        status = ucs_status_to_ucc_status(
            ucp_ep_rkey_unpack(ep, ra[i].packed_key, &ra[i].rkey));
        if (UCC_OK != status) {
            tl_error(tl_team->context->lib,
                     "failed to unpack ra rkey from rank %d", i);
        }
        team->ra[i] = ra[i];
    }
    ucc_free(ra);

    tl_info(tl_team->context->lib, "initialized tl team: %p", team);
    team->status = UCC_OK;
    return UCC_OK;

err_preconnect:
    return status;
}

ucc_status_t ucc_tl_ucp_team_get_scores(ucc_base_team_t   *tl_team,
                                        ucc_coll_score_t **score_p)
{
    ucc_tl_ucp_team_t *team = ucc_derived_of(tl_team, ucc_tl_ucp_team_t);
    ucc_tl_ucp_lib_t  *lib  = UCC_TL_UCP_TEAM_LIB(team);
    ucc_coll_score_t  *score;
    ucc_status_t       status;
    unsigned           i;
    /* There can be a different logic for different coll_type/mem_type.
       Right now just init everything the same way. */
    status = ucc_coll_score_build_default(tl_team, UCC_TL_UCP_DEFAULT_SCORE,
                              ucc_tl_ucp_coll_init, UCC_TL_UCP_SUPPORTED_COLLS,
                              NULL, 0, &score);
    if (UCC_OK != status) {
        return status;
    }
    for (i = 0; i < UCC_TL_UCP_N_DEFAULT_ALG_SELECT_STR; i++) {
        status = ucc_coll_score_update_from_str(
            ucc_tl_ucp_default_alg_select_str[i], score, team->size,
            ucc_tl_ucp_coll_init, &team->super.super, UCC_TL_UCP_DEFAULT_SCORE,
            ucc_tl_ucp_alg_id_to_init);
        if (UCC_OK != status) {
            tl_error(tl_team->context->lib,
                     "failed to apply default coll select setting: %s",
                     ucc_tl_ucp_default_alg_select_str[i]);
            goto err;
        }
    }
    if (strlen(lib->super.super.score_str) > 0) {
        status = ucc_coll_score_update_from_str(
            lib->super.super.score_str, score, team->size, NULL,
            &team->super.super, UCC_TL_UCP_DEFAULT_SCORE,
            ucc_tl_ucp_alg_id_to_init);

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
    return status;
}
