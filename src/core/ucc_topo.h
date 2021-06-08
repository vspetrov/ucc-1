/*
 * Copyright (C) Mellanox Technologies Ltd. 2021.  ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */
#ifndef UCC_TOPO_H_
#define UCC_TOPO_H_
#include "ucc_sbgp.h"
#include "utils/ucc_proc_info.h"

typedef struct ucc_topo {
    ucc_proc_info_t *procs;
    ucc_rank_t       n_procs;
    ucc_rank_t       nnodes;
    ucc_rank_t       min_ppn;
    ucc_rank_t       max_ppn;
    ucc_rank_t       max_n_sockets;
} ucc_topo_t;

typedef struct ucc_team ucc_team_t;
typedef struct ucc_addr_storage ucc_addr_storage_t;
typedef struct ucc_team_topo {
    ucc_topo_t *topo;
    ucc_sbgp_t  sbgps[UCC_SBGP_LAST];
    ucc_rank_t  node_leader_rank;
    ucc_rank_t  node_leader_rank_id;
    int         no_socket;
    ucc_team_t *team;
} ucc_team_topo_t;

ucc_status_t ucc_topo_init(ucc_addr_storage_t *storage, ucc_topo_t **topo);
void ucc_topo_cleanup(ucc_topo_t *topo);

ucc_status_t ucc_team_topo_init(ucc_team_t *team, ucc_topo_t *topo,
                                ucc_team_topo_t **team_topo);
void ucc_team_topo_cleanup(ucc_team_topo_t *team_topo);
ucc_sbgp_t* ucc_team_topo_get_sbgp(ucc_team_topo_t *topo, ucc_sbgp_type_t type);

static inline int ucc_rank_on_local_node(int rank, ucc_topo_t *topo)
{
    return topo->procs[rank].host_hash == ucc_local_proc.host_hash;
}

static inline int ucc_rank_on_local_socket(int rank, ucc_topo_t *topo)
{
    if (ucc_local_proc.socket_id == -1) {
        return 0;
    }
    ucc_proc_info_t *proc = &topo->procs[rank];
    return proc->host_hash == ucc_local_proc.host_hash &&
        proc->socket_id == ucc_local_proc.socket_id;
}

#endif
