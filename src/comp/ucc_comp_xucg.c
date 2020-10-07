/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2019.  ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#include "ucc_comp_xucg.h"

static ucc_status_t xucg_convert_status(ucs_status_t ucx_status)
{
    return UCC_ERR_NOT_IMPLEMENTED;
}

static ucc_status_t xucg_convert_ucc_params(const ucc_lib_params_t *ucc_params,
                                            ucg_params_t *ucg_params)
{
    return UCC_ERR_NOT_IMPLEMENTED;
}

static ucc_status_t xucg_convert_ucc_config(const ucc_lib_config_t *ucc_config,
                                            ucg_config_t *ucg_config)
{
    return UCC_ERR_NOT_IMPLEMENTED;
}

static ucc_status_t xucg_convert_worker_params(const ucc_context_params_t *ucc_params,
                                               ucp_worker_params_t *worker_params)
{
    return UCC_ERR_NOT_IMPLEMENTED;
}

static ucc_status_t xucg_convert_group_params(const ucc_team_params_t *team_params,
                                              ucg_group_params_t *group_params)
{
    return UCC_ERR_NOT_IMPLEMENTED;
}

static ucc_status_t xucg_init(const ucc_lib_params_t *ucc_params,
                              const ucc_lib_config_t *ucc_config,
                              const ucc_tl_lib_config_t *tl_config,
                              ucc_comp_lib_t **comp_lib)
{
    ucs_status_t status;
    ucp_params_t ucp_params;
    ucg_params_t ucg_params;
    ucg_config_t ucg_config;

    ucg_params.super = &ucp_params;
    status = xucg_convert_ucc_params(ucc_params, &ucg_params);
    if (status != UCS_OK) {
        return status;
    }

    status = xucg_convert_ucc_config(ucc_config, &ucg_config);
    if (status != UCS_OK) {
        return status;
    }

    ucc_comp_lib_t *comp_lib = malloc(sizeof(ucc_comp_lib_t));
    if (comp_lib == NULL) {
        return UCC_ERR_NO_MEMORY;
    }

    status = xucg_convert_status(ucg_init(&ucg_params, &ucg_config, &comp_lib->ctx));
    if (status != UCS_OK) {
        free(comp_lib);
        return status;
    }

    return UCC_OK;
}

static void xucg_cleanup(ucc_comp_lib_t *comp_lib)
{
    ucg_cleanup(comp_lib->ctx);
}

static ucc_status_t xucg_context_create(ucc_comp_lib_t *comp_lib,
                                        const ucc_context_params_t *ucc_params,
                                        const ucc_comp_context_config_t *ucc_config,
                                        ucc_comp_context_t **comp_context)
{
    ucc_status_t status;
    ucp_worker_params_t worker_params;

    status = xucg_convert_worker_params(ucc_params, &worker_params);
    if (status != UCS_OK) {
        return status;
    }

    status = xucg_convert_ucc_config(ucc_config, &ucg_config);
    if (status != UCS_OK) {
        return status;
    }

    ucc_comp_context_t *comp_context = malloc(sizeof(ucc_comp_context_t));
    if (comp_context == NULL) {
        return UCC_ERR_NO_MEMORY;
    }

    status = xucg_convert_status(ucp_worker_create(comp_lib->ctx, &ucg_config,
                                                   &comp_context->ctx));
    if (status != UCS_OK) {
        free(comp_lib);
        return status;
    }

    return UCC_OK;
}

static void xucg_context_destroy(ucc_comp_context_t *comp_context)
{
    ucp_worker_destroy(comp_context->ctx);
}

static ucc_status_t xucg_group_create_post_one(ucc_comp_context_t *comp_ctx,
                                               const ucg_group_params_t *params,
                                               ucc_comp_group_t *group)
{
    ucp_worker_h worker = (ucp_worker_h)ucc_comp_context_t->ctx;
    return xucg_convert_status(ucg_group_create(worker, params, &group->ctx));
}

static ucc_status_t xucg_group_create_post(ucc_comp_context_t **comp_ctxs,
                                           unsigned n_ctxs,
                                           const ucc_team_params_t *team_params,
                                           ucc_comp_group_t **group)
{
    ucg_group_params_t group_params;
    ucc_status_t status;
    unsigned idx = 0;

    status = xucg_convert_group_params(team_params, &group_params);
    if (status != UCS_OK) {
        return status;
    }

    do {
        status = xucg_group_create_post_one(comp_ctxs[idx], group_params, group);
    } while ((status == UCC_OK) && (++idx < n_ctxs));

    return status;
}

static ucc_status_t xucg_group_create_test(ucc_comp_group_t *group)
{
    return UCC_OK;
}

static ucc_status_t xucg_group_destroy(ucc_comp_group_t *group)
{
    ucg_group_destroy((ucg_group_h)group->ctx);
    return UCC_OK;
}

ucc_comp_iface_t xucg_component = {
    .name = "xucg",
    .priority = 50,
    // ucc_lib_params_t               params;    // TODO: Why is this here?
    // void*                          dl_handle; // TODO: Why is this here?
    .lib_config = {
            .config_table = xucg_config_table,
            .config_size = sizeof(xucg_config_t)
    },
    .comp_context_config = {
            .config_table = xucg_ctx_config_table,
            .config_size = sizeof(xucg_ctx_config_t)
    },
    .init = xucg_init,
    .cleanup = xucg_cleanup,
    .context_create = xucg_context_create,
    .context_destroy = xucg_context_destroy,
    .group_create_post = xucg_group_create_post,
    .group_create_test = xucg_group_create_test,
    .group_destroy = xucg_group_destroy
};
