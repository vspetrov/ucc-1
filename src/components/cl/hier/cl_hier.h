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

typedef enum {
    UCC_HIER_SBGP_NODE,
    UCC_HIER_SBGP_NODE2,
    UCC_HIER_SBGP_NET,
    UCC_HIER_SBGP_LAST,
} ucc_hier_sbgp_type_t;

typedef struct ucc_cl_hier_lib_config {
    ucc_cl_lib_config_t super;
    uint32_t            allreduce_hybrid_n_frags;
    uint32_t            allreduce_hybrid_pipeline_depth;
    size_t              allreduce_hybrid_frag_thresh;
    size_t              allreduce_hybrid_frag_size;
    ucc_config_names_array_t sbgp_tls[UCC_HIER_SBGP_LAST];
} ucc_cl_hier_lib_config_t;

typedef struct ucc_cl_hier_context_config {
    ucc_cl_context_config_t super;
} ucc_cl_hier_context_config_t;

typedef struct ucc_cl_hier_lib {
    ucc_cl_lib_t super;
    ucc_cl_hier_lib_config_t cfg;
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
    UCC_HIER_SBGP_DISABLED,
    UCC_HIER_SBGP_ENABLED
} ucc_hier_sbgp_state_t;

#define CL_HIER_MAX_SBGP_TLS 4

typedef struct ucc_hier_sbgp {
    ucc_hier_sbgp_state_t     state;
    ucc_sbgp_type_t           sbgp_type;
    ucc_sbgp_t               *sbgp;
    ucc_score_map_t          *score_map;
    ucc_coll_score_t         *score;
    ucc_tl_team_t            *tl_teams[CL_HIER_MAX_SBGP_TLS];
    ucc_tl_context_t         *tl_ctxs[CL_HIER_MAX_SBGP_TLS];
    int                       n_tls;
} ucc_hier_sbgp_t;

typedef struct ucc_cl_hier_team {
    ucc_cl_team_t            super;
    ucc_team_multiple_req_t *team_create_req;
    unsigned                 n_tl_teams;
    ucc_coll_score_t        *score;
    ucc_hier_sbgp_t          sbgps[UCC_HIER_SBGP_LAST];
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

#define UCC_CL_HIER_TEAM_LIB(_team)                                             \
    (ucc_derived_of((_team)->super.super.context->lib, ucc_cl_hier_lib_t))

#endif
