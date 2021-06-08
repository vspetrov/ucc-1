/**
 * Copyright (C) Mellanox Technologies Ltd. 2020-2021.  ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#ifndef UCC_TEAM_H_
#define UCC_TEAM_H_

#include "ucc/api/ucc.h"
#include "utils/ucc_datastruct.h"
#include "ucc_context.h"
#include "utils/ucc_math.h"

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
} ucc_team_t;

/* If the bit is set then team_id is provided by the user */
#define UCC_TEAM_ID_EXTERNAL_BIT ((uint16_t)UCC_BIT(15))
#define UCC_TEAM_ID_IS_EXTERNAL(_team) (team->id & UCC_TEAM_ID_EXTERNAL_BIT)
#define UCC_TEAM_ID_MAX ((uint16_t)UCC_BIT(15) - 1)

void ucc_copy_team_params(ucc_team_params_t *dst, const ucc_team_params_t *src);

static inline ucc_context_addr_header_t*
ucc_get_team_ep_header(ucc_context_t *context, ucc_team_t *team, ucc_rank_t rank)
{
    return (ucc_context_addr_header_t*)
        PTR_OFFSET(team->addr_storage.storage, team->addr_storage.addr_len * rank);
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

#endif
