/**
 * Copyright (C) Mellanox Technologies Ltd. 2020.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#ifndef UCC_CL_HIER_H_
#define UCC_CL_HIER_H_
#include "components/cl/ucc_cl.h"
#include "components/cl/ucc_cl_log.h"
#include "components/tl/ucc_tl.h"
#include "coll_score/ucc_coll_score.h"

#ifndef UCC_CL_HIER_DEFAULT_SCORE
#define UCC_CL_HIER_DEFAULT_SCORE 100
#endif

typedef struct ucc_cl_hier_iface {
    ucc_cl_iface_t super;
} ucc_cl_hier_iface_t;
/* Extern iface should follow the pattern: ucc_cl_<cl_name> */
extern ucc_cl_hier_iface_t ucc_cl_hier;

typedef struct ucc_cl_hier_lib_config {
    ucc_cl_lib_config_t super;
} ucc_cl_hier_lib_config_t;

typedef struct ucc_cl_hier_context_config {
    ucc_cl_context_config_t super;
} ucc_cl_hier_context_config_t;

typedef struct ucc_cl_hier_lib {
    ucc_cl_lib_t super;
} ucc_cl_hier_lib_t;
UCC_CLASS_DECLARE(ucc_cl_hier_lib_t, const ucc_base_lib_params_t *,
                  const ucc_base_config_t *);

typedef struct ucc_cl_hier_context {
    ucc_cl_context_t   super;
    ucc_tl_context_t **tl_ctxs;
    unsigned           n_tl_ctxs;
} ucc_cl_hier_context_t;
UCC_CLASS_DECLARE(ucc_cl_hier_context_t, const ucc_base_context_params_t *,
                  const ucc_base_config_t *);

typedef enum {
    UCC_HIER_PAIR_DISABLED,
    UCC_HIER_PAIR_ENABLED
} ucc_hier_pair_state_t;

typedef struct ucc_hier_pair {
    ucc_hier_pair_state_t state;
    ucc_tl_team_t *tl_team;
    ucc_tl_context_t *tl_ctx;
    ucc_sbgp_t    *sbgp;
    ucc_score_map_t *score_map;
} ucc_hier_pair_t;

typedef enum {
    UCC_HIER_PAIR_NODE_UCP,
    UCC_HIER_PAIR_NET_UCP,
    UCC_HIER_PAIR_LAST,
} ucc_hier_pair_type_t;

typedef struct ucc_cl_hier_team {
    ucc_cl_team_t            super;
    ucc_team_multiple_req_t *team_create_req;
    ucc_tl_team_t          **tl_teams;
    unsigned                 n_tl_teams;
    ucc_coll_score_t        *score;
    ucc_score_map_t         *score_map;
    ucc_hier_pair_t          pairs[UCC_HIER_PAIR_LAST];
} ucc_cl_hier_team_t;
UCC_CLASS_DECLARE(ucc_cl_hier_team_t, ucc_base_context_t *,
                  const ucc_base_team_params_t *);

ucc_status_t ucc_cl_hier_allreduce_init(ucc_base_coll_args_t *coll_args,
                                    ucc_base_team_t *team,
                                    ucc_coll_task_t **task);
ucc_status_t ucc_cl_hier_coll_init(ucc_base_coll_args_t *coll_args,
                                    ucc_base_team_t *team,
                                    ucc_coll_task_t **task);

#define UCC_CL_HIER_TEAM_CTX(_team)                                           \
    (ucc_derived_of((_team)->super.super.context, ucc_cl_hier_context_t))

#endif
