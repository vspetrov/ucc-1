/*
 * Copyright (C) Mellanox Technologies Ltd. 2021.  ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#include "config.h"
#include "ucc_topo.h"
#include "ucc_context.h"
#include "utils/ucc_malloc.h"
#include "utils/ucc_math.h"
#include <string.h>
#include <limits.h>

int ucc_topo_compare_proc_info(const void* a, const void* b)
{
    const ucc_proc_info_t *d1 = (const ucc_proc_info_t*)a;
    const ucc_proc_info_t *d2 = (const ucc_proc_info_t*)b;
    if (d1->host_hash != d2->host_hash) {
        return d1->host_hash > d2->host_hash ? 1 : -1;
    } else if (d1->socket_id != d2->socket_id) {
        return d1->socket_id - d2->socket_id;
    } else {
        return d1->pid - d2->pid;
    }
}

static void compute_layout(ucc_topo_t *topo, ucc_rank_t size) {
    ucc_proc_info_t *sorted = (ucc_proc_info_t*)
        malloc(size*sizeof(ucc_proc_info_t));
    memcpy(sorted, topo->procs, size*sizeof(ucc_proc_info_t));
    qsort(sorted, size, sizeof(ucc_proc_info_t),
          ucc_topo_compare_proc_info);
    unsigned long current_hash = sorted[0].host_hash;
    int current_ppn = 1;
    int min_ppn = INT_MAX;
    int max_ppn = 0;
    int nnodes = 1;
    int max_sockid = 0;
    int i, j;
    for (i=1; i<size; i++) {
        unsigned long hash = sorted[i].host_hash;
        if (hash != current_hash) {
            for (j=0; j<size; j++) {
                if (topo->procs[j].host_hash == current_hash) {
                    topo->procs[j].host_id = nnodes - 1;
                }
            }
            if (current_ppn > max_ppn) max_ppn = current_ppn;
            if (current_ppn < min_ppn) min_ppn = current_ppn;
            nnodes++;
            current_hash = hash;
            current_ppn = 1;
        } else {
            current_ppn++;
        }
    }
    for (j=0; j<size; j++) {
        if (topo->procs[j].socket_id > max_sockid) {
            max_sockid = topo->procs[j].socket_id;
        }
        if (topo->procs[j].host_hash == current_hash) {
            topo->procs[j].host_id = nnodes - 1;
        }
    }

    if (current_ppn > max_ppn) max_ppn = current_ppn;
    if (current_ppn < min_ppn) min_ppn = current_ppn;
    free(sorted);
    topo->nnodes = nnodes;
    topo->min_ppn = min_ppn;
    topo->max_ppn = max_ppn;
    topo->max_n_sockets = max_sockid+1;
}

ucc_status_t ucc_topo_init(ucc_addr_storage_t *storage, ucc_topo_t **_topo)
{
    ucc_context_addr_header_t *h;
    ucc_topo_t *topo = malloc(sizeof(*topo));
    int i;
    if (!topo) {
        return UCC_ERR_NO_MEMORY;
    }
    topo->n_procs = storage->size;
    topo->procs = (ucc_proc_info_t*)malloc(storage->size*sizeof(ucc_proc_info_t));
    for (i = 0; i < storage->size; i++) {
        h = (ucc_context_addr_header_t*)PTR_OFFSET(storage->storage,
                                                   storage->addr_len * i);
        topo->procs[i] = h->ctx_id.pi;
    }
    compute_layout(topo, storage->size);
    *_topo = topo;
    return UCC_OK;
}

void ucc_topo_cleanup(ucc_topo_t *topo)
{
    if (topo) {
        free(topo->procs);
        free(topo);
    }
}

ucc_status_t ucc_team_topo_init(ucc_team_t *team, ucc_topo_t *topo, ucc_team_topo_t **_team_topo)
{
    ucc_team_topo_t *team_topo = malloc(sizeof(*team_topo));
    int i;
    if (!topo) {
        return UCC_ERR_NO_MEMORY;
    }
    team_topo->topo = topo;
    for (i=0; i<UCC_SBGP_LAST; i++) {
        team_topo->sbgps[i].status = UCC_SBGP_NOT_INIT;
    }
    team_topo->no_socket = 0;
    team_topo->node_leader_rank = -1;
    team_topo->node_leader_rank_id = 0;
    team_topo->team = team;
    *_team_topo = team_topo;
    return UCC_OK;
}

void ucc_team_topo_cleanup(ucc_team_topo_t *team_topo)
{
    int i;
    if (team_topo) {
        for (i=0; i<UCC_SBGP_LAST; i++) {
            if (team_topo->sbgps[i].status == UCC_SBGP_ENABLED) {
                ucc_sbgp_cleanup(&team_topo->sbgps[i]);
            }
        }
        free(team_topo);
    }
}

ucc_sbgp_t* ucc_team_topo_get_sbgp(ucc_team_topo_t *topo, ucc_sbgp_type_t type)
{
    if (topo->sbgps[type].status == UCC_SBGP_NOT_INIT) {
        ucc_sbgp_create(topo, type);
    }
    return &topo->sbgps[type];
}
