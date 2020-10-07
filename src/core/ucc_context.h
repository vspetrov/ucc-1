/*
* Copyright (C) Mellanox Technologies Ltd. 2001-2020.  ALL RIGHTS RESERVED.
* See file LICENSE for terms.
*/

#ifndef UCC_CONTEXT_H_
#define UCC_CONTEXT_H_

#include <api/ucc.h>

typedef struct ucc_lib_info ucc_lib_info_t;
typedef struct ucc_tl_context ucc_tl_context_t;
typedef struct ucc_tl_context_config ucc_tl_context_config_t;
typedef struct ucc_comp_context ucc_comp_context_t;
typedef struct ucc_comp_context_config ucc_comp_context_config_t;

typedef struct ucc_context {
    ucc_lib_info_t        *lib;
    ucc_context_params_t  params;
    ucc_tl_context_t      **tl_ctx;
    unsigned              n_tl_ctx;
    ucc_comp_context_t    **comp_ctx;
    unsigned              n_comp_ctx;
} ucc_context_t;

typedef struct ucc_context_config {
    ucc_lib_info_t            *lib;
    ucc_tl_context_config_t   **tl_configs;
    unsigned                  n_tl_cfg;
    ucc_comp_context_config_t **comp_configs;
    unsigned                  n_comp_cfg;
} ucc_context_config_t;

#endif
