/**
 * Copyright (C) Mellanox Technologies Ltd. 2020-2021.  ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#ifndef UCC_TEAM_H_
#define UCC_TEAM_H_

#include "ucc/api/ucc.h"
#include "utils/ucc_datastruct.h"
#include "utils/ucc_coll_utils.h"
#include "ucc_context.h"
#include "utils/ucc_math.h"
#include "ucc_topo.h"
#include "coll_score/ucc_coll_score.h"

typedef struct ucc_context ucc_context_t;
typedef struct ucc_cl_team ucc_cl_team_t;
typedef struct ucc_tl_team ucc_tl_team_t;
typedef struct ucc_coll_task ucc_coll_task_t;
typedef enum {
    UCC_TEAM_ADDR_EXCHANGE,    
    UCC_TEAM_SERVICE_TEAM,
    UCC_TEAM_ALLOC_ID,
    UCC_TEAM_CL_CREATE,
} ucc_team_state_t;

typedef struct ucc_team {
    ucc_status_t       status;
    ucc_team_state_t   state;
    ucc_context_t    **contexts;
    uint32_t           num_contexts;
    ucc_team_params_t  params;
    ucc_cl_team_t    **cl_teams;
    int                n_cl_teams;
    int                last_team_create_posted;
    uint16_t           id; /*< context-uniq team identifier */
    ucc_rank_t         rank;
    ucc_rank_t         size;
    ucc_tl_team_t     *service_team;
    ucc_coll_task_t   *task;
    ucc_addr_storage_t addr_storage; /*< addresses of team endpoints */
    ucc_rank_t         *ctx_ranks;
    ucc_rank_t          ctx_rank;
    void               *oob_req;
    ucc_ep_map_t        ctx_map;
    ucc_team_topo_t    *topo;
    ucc_score_map_t    *score_map;
} ucc_team_t;

/* If the bit is set then team_id is provided by the user */
#define UCC_TEAM_ID_EXTERNAL_BIT ((uint16_t)UCC_BIT(15))
#define UCC_TEAM_ID_IS_EXTERNAL(_team) (team->id & UCC_TEAM_ID_EXTERNAL_BIT)
#define UCC_TEAM_ID_MAX ((uint16_t)UCC_BIT(15) - 1)

void ucc_copy_team_params(ucc_team_params_t *dst, const ucc_team_params_t *src);

static inline ucc_context_addr_header_t*
ucc_get_team_ep_header(ucc_context_t *context, ucc_team_t *team, ucc_rank_t rank)
{
    ucc_addr_storage_t *storage = context->addr_storage.storage ?
        &context->addr_storage : &team->addr_storage;
    ucc_rank_t storage_rank = context->addr_storage.storage ?
        ucc_ep_map_eval(team->ctx_map, rank) : rank;

    return UCC_ADDR_STORAGE_RANK_HEADER(storage, storage_rank);
}

static inline void*
ucc_get_team_ep_addr(ucc_context_t *context, ucc_team_t *team, ucc_rank_t rank,
                     unsigned long component_id)
{
    ucc_context_addr_header_t *h = ucc_get_team_ep_header(context, team, rank);
    int i;
    for (i = 0; i < h->n_components; i++) {
        if (h->components[i].id == component_id) {
            return PTR_OFFSET(h, h->components[i].offset);
        }
    }
    ucc_assert(0);
    return NULL;
}

static inline int ucc_rank_on_local_node(int team_rank, ucc_team_t *team)
{
    ucc_proc_info_t *procs    = team->topo->topo->procs;
    ucc_rank_t       ctx_rank = ucc_ep_map_eval(team->ctx_map, team_rank);
    return procs[ctx_rank].host_hash == ucc_local_proc.host_hash;
}

static inline int ucc_rank_on_local_socket(int team_rank, ucc_team_t *team)
{
    ucc_rank_t       ctx_rank = ucc_ep_map_eval(team->ctx_map, team_rank);
    ucc_proc_info_t *proc     = &team->topo->topo->procs[ctx_rank];

    if (ucc_local_proc.socket_id == -1) {
        return 0;
    }
    return proc->host_hash == ucc_local_proc.host_hash &&
        proc->socket_id == ucc_local_proc.socket_id;
}

ucc_status_t ucc_coll_triggered_post_common(ucc_ee_h ee, ucc_ev_t *ev, //NOLINT
                                            ucc_coll_task_t *coll_task);

#endif
