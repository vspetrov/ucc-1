/*
 * Copyright (C) Mellanox Technologies Ltd. 2001-2020.  ALL RIGHTS RESERVED.
 * Copyright (C) Huawei Technologies Co., Ltd. 2019.  ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#ifndef UCC_COMP_H_
#define UCC_COMP_H_

#include "config.h"
#include "api/ucc.h"
#include "core/ucc_lib.h"
#include "core/ucc_context.h"
#include <ucs/config/types.h>
#include <ucs/debug/log_def.h>
#include <ucs/config/parser.h>
#include <assert.h>
#include <string.h>

typedef struct ucc_comp_lib     ucc_comp_lib_t;
typedef struct ucc_comp_iface   ucc_comp_iface_t;
typedef struct ucc_comp_context ucc_comp_context_t;
typedef struct ucc_comp_team    ucc_comp_team_t;

typedef struct ucc_comp_lib_config {
    /* Log level above which log messages will be printed */
    ucs_log_component_config_t log_component;
    /* Team library priority */
    int                        priority;
} ucc_comp_lib_config_t;
extern ucs_config_field_t ucc_comp_lib_config_table[];

typedef struct ucc_comp_context_config {
    ucc_comp_iface_t *iface;
    ucc_comp_lib_t *tl_lib;
} ucc_comp_context_config_t;
extern ucs_config_field_t ucc_comp_context_config_table[];

typedef struct ucc_comp_iface {
    char*                          name;
    int                            priority;
    ucc_lib_params_t               params;
    void*                          dl_handle;
    ucs_config_global_list_entry_t tl_lib_config;
    ucs_config_global_list_entry_t tl_context_config;
    ucc_status_t                   (*init)(const ucc_lib_params_t *params,
                                           const ucc_lib_config_t *config,
                                           const ucc_comp_lib_config_t *tl_config,
                                           ucc_comp_lib_t **tl_lib);
    void                           (*cleanup)(ucc_comp_lib_t *tl_lib);
    ucc_status_t                   (*context_create)(ucc_comp_lib_t *tl_lib,
                                                     const ucc_context_params_t *params,
                                                     const ucc_comp_context_config_t *config,
                                                     ucc_comp_context_t **tl_context);
    void                           (*context_destroy)(ucc_comp_context_t *tl_context);
    ucc_status_t                   (*team_create_post)(ucc_comp_context_t **tl_ctxs,
                                                       uint32_t n_ctxs,
                                                       const ucc_team_params_t *params,
                                                       ucc_comp_team_t **team);
    ucc_status_t                   (*team_create_test)(ucc_comp_team_t *tneam_ctx);
    ucc_status_t                   (*team_destroy)(ucc_comp_team_t *team);
} ucc_comp_iface_t;

typedef struct ucc_comp_lib {
    ucc_comp_iface_t *iface;
    int              priority;
    void             *ctx;
} ucc_comp_lib_t;

#endif
