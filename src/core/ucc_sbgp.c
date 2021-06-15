/*
* Copyright (C) Mellanox Technologies Ltd. 2021.  ALL RIGHTS RESERVED.
* See file LICENSE for terms.
*/

#include "ucc_sbgp.h"
#include "ucc_topo.h"
#include "ucc_team.h"
#include "utils/ucc_compiler_def.h"
#include <limits.h>

char* ucc_sbgp_type_str[UCC_SBGP_LAST] = {"undef", "numa", "socket", "node", "node_leaders",
                                          "net", "socket_leaders", "numa_leaders", "flat"};

#define SWAP(_x, _y) do{                        \
        int _tmp   = (_x);                      \
        (_x)       = (_y);                      \
        (_y)       = _tmp;                      \
    } while(0)

static inline ucc_status_t sbgp_create_socket(ucc_team_topo_t *topo, ucc_sbgp_t *sbgp)
{
    ucc_team_t *team = sbgp->team;
    ucc_sbgp_t *node_sbgp = &topo->sbgps[UCC_SBGP_NODE];
    int group_rank    = team->rank;
    int nlr           = topo->node_leader_rank;
    int sock_rank = 0, sock_size = 0, i, r, nlr_pos;
    int *local_ranks;

    ucc_assert(node_sbgp->status == UCC_SBGP_ENABLED);
    local_ranks = (int*)malloc(node_sbgp->group_size*sizeof(int));
    if (!local_ranks) {
        return UCC_ERR_NO_MEMORY;
    }
    for (i=0; i<node_sbgp->group_size; i++) {
        r = node_sbgp->rank_map[i];
        if (ucc_rank_on_local_socket(r, team)) {
            local_ranks[sock_size] = r;
            if (r == group_rank) {
                sock_rank = sock_size;
            }
            sock_size++;
        }
    }
    sbgp->group_size = sock_size;
    sbgp->group_rank = sock_rank;
    sbgp->rank_map = local_ranks;
    nlr_pos = -1;
    for (i=0; i<sock_size; i++) {
        if (nlr == local_ranks[i]) {
            nlr_pos = i;
            break;
        }
    }
    if (nlr_pos > 0) {
        if (sock_rank == 0) sbgp->group_rank = nlr_pos;
        if (sock_rank == nlr_pos) sbgp->group_rank = 0;
        SWAP(local_ranks[nlr_pos], local_ranks[0]);
    }
    if (sock_size > 1) {
        sbgp->status = UCC_SBGP_ENABLED;
    } else {
        sbgp->status = UCC_SBGP_NOT_EXISTS;
    }
    return UCC_OK;
}

static inline ucc_status_t sbgp_create_node(ucc_team_topo_t *topo, ucc_sbgp_t *sbgp)
{
    ucc_team_t *team        = sbgp->team;
    int group_size           = team->size;
    int group_rank           = team->rank;
    int max_local_size       = 256;
    int ctx_nlr              = topo->node_leader_rank_id;
    int node_rank = 0, node_size = 0, i;
    int *local_ranks;
    local_ranks = (int*)malloc(max_local_size*sizeof(int));
    if (!local_ranks) {
        return UCC_ERR_NO_MEMORY;
    }
    for (i=0; i<group_size; i++) {
        if (ucc_rank_on_local_node(i, team)) {
            if (node_size == max_local_size) {
                max_local_size *= 2;
                local_ranks = (int*)realloc(local_ranks, max_local_size*sizeof(int));
            }
            local_ranks[node_size] = i;

            if (i == group_rank) {
                node_rank = node_size;
            }
            node_size++;
        }
    }
    sbgp->group_size = node_size;
    sbgp->group_rank = node_rank;
    sbgp->rank_map = local_ranks;
    if (0 < ctx_nlr && ctx_nlr < node_size)  {
        /* Rotate local_ranks array so that node_leader_rank_id becomes first
           in that array */
        sbgp->rank_map = (int*)malloc(node_size*sizeof(int));
        if (!sbgp->rank_map) {
            free(local_ranks);
            return UCC_ERR_NO_MEMORY;
        }
        for (i=ctx_nlr; i<node_size; i++) {
            sbgp->rank_map[i - ctx_nlr] = local_ranks[i];
        }

        for (i=0; i<ctx_nlr; i++) {
            sbgp->rank_map[node_size - ctx_nlr + i] = local_ranks[i];
        }
        sbgp->group_rank = (node_rank + node_size - ctx_nlr) % node_size;
        free(local_ranks);
    }
    topo->node_leader_rank = sbgp->rank_map[0];
    if (node_size > 1) {
        sbgp->status = UCC_SBGP_ENABLED;
    } else {
        sbgp->status = UCC_SBGP_NOT_EXISTS;
    }
    return UCC_OK;
}

static ucc_status_t sbgp_create_node_leaders(ucc_team_topo_t *topo, ucc_sbgp_t *sbgp,
                                             int ctx_nlr)
{
    ucc_team_t *team   = sbgp->team;
    int comm_size        = team->size;
    int comm_rank        = team->rank;
    int i_am_node_leader = 0;
    int nnodes           = topo->topo->nnodes;
    int i, n_node_leaders;
    int *nl_array_1, *nl_array_2;

    nl_array_1 = (int*)malloc(nnodes*sizeof(int));
    if (!nl_array_1) {
        return UCC_ERR_NO_MEMORY;
    }
    nl_array_2 = (int*)malloc(nnodes*sizeof(int));
    if (!nl_array_2) {
        free(nl_array_1);
        return UCC_ERR_NO_MEMORY;
    }

    for (i=0; i<nnodes; i++) {
        nl_array_1[i] = 0;
        nl_array_2[i] = INT_MAX;
    }

    for (i=0; i<comm_size; i++) {
        int ctx_rank = ucc_ep_map_eval(team->ctx_map, i);
        int host_id = topo->topo->procs[ctx_rank].host_id;
        if (nl_array_1[host_id] == 0 ||
            nl_array_1[host_id] == ctx_nlr) {
            nl_array_2[host_id] = i;
        }
        nl_array_1[host_id]++;
    }
    n_node_leaders = 0;
    for (i=0; i<nnodes; i++) {
        if (nl_array_2[i] != INT_MAX) {
            if (comm_rank == nl_array_2[i]) {
                i_am_node_leader = 1;
                sbgp->group_rank = n_node_leaders;
            }
            nl_array_1[n_node_leaders++] = nl_array_2[i];
        }
    }
    free(nl_array_2);

    if (n_node_leaders > 1) {
        if (i_am_node_leader) {
            sbgp->group_size = n_node_leaders;
            sbgp->rank_map = nl_array_1;
            sbgp->status = UCC_SBGP_ENABLED;
        } else {
            free(nl_array_1);
            sbgp->status = UCC_SBGP_DISABLED;
        }
    } else {
        free(nl_array_1);
        sbgp->status = UCC_SBGP_NOT_EXISTS;
    }
    return UCC_OK;
}

static ucc_status_t sbgp_create_socket_leaders(ucc_team_topo_t *topo, ucc_sbgp_t *sbgp)
{
    ucc_team_t *team              = sbgp->team;
    ucc_sbgp_t *node_sbgp         = &topo->sbgps[UCC_SBGP_NODE];
    int         comm_rank          = team->rank;
    int         nlr                = topo->node_leader_rank;
    int         i_am_socket_leader = (nlr == comm_rank);
    int         max_n_sockets      = topo->topo->max_n_sockets;
    int         *sl_array          = (int*)malloc(max_n_sockets*sizeof(int));
    int         n_socket_leaders   = 1, i, nlr_sock_id;

    if (!sl_array) {
        return UCC_ERR_NO_MEMORY;
    }
    for (i=0; i<max_n_sockets; i++) {
        sl_array[i] = INT_MAX;
    }
    nlr_sock_id = topo->topo->procs[ucc_ep_map_eval(team->ctx_map, nlr)].socket_id;
    sl_array[nlr_sock_id] = nlr;

    for (i=0; i<node_sbgp->group_size; i++) {
        int r = node_sbgp->rank_map[i];
        int ctx_rank = ucc_ep_map_eval(team->ctx_map, r);
        int socket_id = topo->topo->procs[ctx_rank].socket_id;
        if (sl_array[socket_id] == INT_MAX) {
            n_socket_leaders++;
            sl_array[socket_id] = r;
            if (r == comm_rank) {
                i_am_socket_leader = 1;
            }
        }
    }

    if (n_socket_leaders > 1) {
        if (i_am_socket_leader) {
            int sl_rank = -1;
            sbgp->rank_map = (int*)malloc(sizeof(int)*n_socket_leaders);
            if (!sbgp->rank_map) {
                free(sl_array);
                return UCC_ERR_NO_MEMORY;
            }
            n_socket_leaders = 0;
            for (i=0; i<max_n_sockets; i++) {
                if (sl_array[i] != INT_MAX) {
                    sbgp->rank_map[n_socket_leaders] = sl_array[i];
                    if (comm_rank == sl_array[i]) {
                        sl_rank = n_socket_leaders;
                    }
                    n_socket_leaders++;
                }
            }
            int nlr_pos = -1;
            for (i=0; i<n_socket_leaders; i++) {
                if (sbgp->rank_map[i] == nlr) {
                    nlr_pos = i;
                    break;
                }
            }
	    ucc_assert(sl_rank >= 0);
            ucc_assert(nlr_pos >= 0);
            sbgp->group_rank = sl_rank;
            if (nlr_pos > 0) {
                if (sl_rank == 0) sbgp->group_rank = nlr_pos;
                if (sl_rank == nlr_pos) sbgp->group_rank = 0;
                SWAP(sbgp->rank_map[nlr_pos], sbgp->rank_map[0]);
            }

            sbgp->group_size = n_socket_leaders;
            sbgp->status = UCC_SBGP_ENABLED;
        } else {
            sbgp->status = UCC_SBGP_DISABLED;
        }
    } else {
        sbgp->status = UCC_SBGP_NOT_EXISTS;
    }
    free(sl_array);
    return UCC_OK;
}

ucc_status_t ucc_sbgp_create(ucc_team_topo_t *topo, ucc_sbgp_type_t type)
{
    ucc_status_t status = UCC_OK;
    ucc_team_t *team = topo->team;
    ucc_sbgp_t  *sbgp = &topo->sbgps[type];

    sbgp->team   = team;
    sbgp->type   = type;
    sbgp->status = UCC_SBGP_NOT_EXISTS;

    switch(type) {
    case UCC_SBGP_NODE:
        status = sbgp_create_node(topo, sbgp);
        break;
    case UCC_SBGP_SOCKET:
        if (topo->sbgps[UCC_SBGP_NODE].status == UCC_SBGP_NOT_INIT) {
            ucc_sbgp_create(topo, UCC_SBGP_NODE);
        }
        if (topo->sbgps[UCC_SBGP_NODE].status == UCC_SBGP_ENABLED) {
            status = sbgp_create_socket(topo, sbgp);
        }
        break;
    case UCC_SBGP_NODE_LEADERS:
        ucc_assert(UCC_SBGP_DISABLED != topo->sbgps[UCC_SBGP_NODE].status);
        status = sbgp_create_node_leaders(topo, sbgp, topo->node_leader_rank_id);
        break;
    case UCC_SBGP_NET:
        if (topo->sbgps[UCC_SBGP_NODE].status == UCC_SBGP_NOT_INIT) {
            ucc_sbgp_create(topo, UCC_SBGP_NODE);
        }
        ucc_assert(UCC_SBGP_DISABLED != topo->sbgps[UCC_SBGP_NODE].status);
        status = sbgp_create_node_leaders(topo, sbgp,
                                          topo->sbgps[UCC_SBGP_NODE].group_rank);
        break;
    case UCC_SBGP_SOCKET_LEADERS:
        if (topo->sbgps[UCC_SBGP_NODE].status == UCC_SBGP_NOT_INIT) {
            ucc_sbgp_create(topo, UCC_SBGP_NODE);
        }
        if (topo->sbgps[UCC_SBGP_NODE].status == UCC_SBGP_ENABLED) {
            status = sbgp_create_socket_leaders(topo, sbgp);
        }
        break;
    default:
        status = UCC_ERR_NOT_IMPLEMENTED;
        break;
    };
    return status;
}

ucc_status_t ucc_sbgp_cleanup(ucc_sbgp_t *sbgp)
{
    if (sbgp->rank_map) {
        free(sbgp->rank_map);
    }
    return UCC_OK;
}

void ucc_sbgp_print(ucc_sbgp_t *sbgp)
{
    int i;
    if (sbgp->group_rank == 0 && sbgp->status == UCC_SBGP_ENABLED) {
        printf("sbgp: %15s: group_size %4d, team_ranks=[ ",
               ucc_sbgp_type_str[sbgp->type], sbgp->group_size);
        for (i=0; i<sbgp->group_size; i++) {
            printf("%d ", sbgp->rank_map[i]);
        }
        printf("]");
        printf("\n");
    }
}


