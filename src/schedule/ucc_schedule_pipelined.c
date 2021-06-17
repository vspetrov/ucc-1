/**
 * Copyright (C) Mellanox Technologies Ltd. 2021.  ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */
#include "ucc_schedule.h"
#include "ucc_schedule_pipelined.h"

ucc_status_t ucc_frag_start_handler(ucc_coll_task_t *parent, /* NOLINT */
                                    ucc_coll_task_t *task)
{
    ucc_schedule_pipelined_t *schedule = ucc_derived_of(parent, ucc_schedule_pipelined_t);
    ucc_schedule_t *frag = ucc_derived_of(task, ucc_schedule_t);
    ucc_status_t status;
    if(schedule->frag_setup) {
        status = schedule->frag_setup(schedule, frag, schedule->n_frags_started);
        if (UCC_OK != status) {
            ucc_error("failed to setup fragment %d of pipelined schedule",
                      schedule->n_frags_started);
            return status;
        }
    }
    /* printf("started frag %p, frag_num %d, to_post %d, next to post %d %p status %s\n", */
    /*        frag, schedule->n_frags_started, schedule->next_frag_to_post, */
    /*        (schedule->next_frag_to_post + 1 ) % schedule->n_frags, */
    /*        schedule->frags[(schedule->next_frag_to_post + 1 ) % schedule->n_frags], */
    /*        ucc_status_string(schedule->frags[(schedule->next_frag_to_post + 1 ) % schedule->n_frags]->super.super.status)); */
    schedule->next_frag_to_post =
        (schedule->next_frag_to_post + 1 ) % schedule->n_frags;
    schedule->n_frags_started++;
    return task->post(task);
}

static ucc_status_t
ucc_schedule_pipelined_completed_handler(ucc_coll_task_t *parent_task, //NOLINT
                                         ucc_coll_task_t *task)
{
    ucc_schedule_pipelined_t *self = ucc_container_of(task, ucc_schedule_pipelined_t, super);
    ucc_schedule_t *frag = ucc_derived_of(parent_task, ucc_schedule_t);
    int i;
    self->super.n_completed_tasks += 1;
    self->n_frags_in_pipeline--;
    /* printf("completed frag %p, n_completed %d, n_started %d, n_total %d\n", */
           /* frag, self->super.n_completed_tasks, self->n_frags_started, self->super.n_tasks); */
    ucc_assert(frag->super.super.status == UCC_OK);
    if (self->super.n_completed_tasks == self->super.n_tasks) {
        self->super.super.super.status = UCC_OK;
        ucc_event_manager_notify(&self->super.super, UCC_EVENT_COMPLETED);
        return UCC_OK;
    }
    while ((self->super.n_completed_tasks + self->n_frags_in_pipeline <
            self->super.n_tasks) && (frag->super.super.status == UCC_OK)){
        /* need to post more frags*/
        if (frag == self->frags[self->next_frag_to_post]) {
            /* printf("restarting frag %d %p\n", self->next_frag_to_post, frag); */
            frag->super.super.status   = UCC_OPERATION_INITIALIZED;
            frag->n_completed_tasks    = 0;
            for (i = 0; i<frag->n_tasks; i++) {
                frag->tasks[i]->n_deps += frag->tasks[i]->n_deps_orig;
                frag->tasks[i]->super.status = UCC_OPERATION_INITIALIZED;
            }
            self->n_frags_in_pipeline++;
            ucc_frag_start_handler(&self->super.super, &frag->super);
        }
        frag = self->frags[self->next_frag_to_post];
        if (&frag->super == parent_task) {
            break;
        }
    }
    return UCC_OK;
}

static ucc_status_t
ucc_schedule_pipelined_completed_handler_seq(ucc_coll_task_t *parent_task, //NOLINT
                                             ucc_coll_task_t *task)
{
    ucc_schedule_pipelined_t *self = ucc_container_of(task, ucc_schedule_pipelined_t, super);
    ucc_schedule_t *frag = ucc_derived_of(parent_task, ucc_schedule_t);

    self->super.n_completed_tasks += 1;
    self->n_frags_in_pipeline--;
    /* printf("completed frag %p, n_completed %d, n_started %d, n_total %d\n", */
           /* frag, self->super.n_completed_tasks, self->n_frags_started, self->super.n_tasks); */
    if (self->super.n_completed_tasks == self->super.n_tasks) {
        self->super.super.super.status = UCC_OK;
        ucc_event_manager_notify(&self->super.super, UCC_EVENT_COMPLETED);
    } else if (self->super.n_completed_tasks + self->n_frags_in_pipeline <
               self->super.n_tasks){
        /* need to post more frags*/

        int i;
#if 0
        ucc_assert(frag == self->frags[0]);
        for (i = 1; i < self->n_frags; i++) {
            self->frags[i-1] = self->frags[i];
        }
        self->frags[i-1] = frag;
#endif
        frag->super.super.status   = UCC_OPERATION_INITIALIZED;
        frag->n_completed_tasks    = 0;
        for (i = 0; i<frag->n_tasks; i++) {
            frag->tasks[i]->n_deps += frag->tasks[i]->n_deps_orig;
            frag->tasks[i]->super.status = UCC_OPERATION_INITIALIZED;
        }
        self->n_frags_in_pipeline++;
        ucc_frag_start_handler(&self->super.super, &frag->super);
    }
    return UCC_OK;
}

ucc_status_t ucc_schedule_pipelined_finalize(ucc_coll_task_t *task)
{
    int i;
    ucc_schedule_pipelined_t *schedule_p = ucc_derived_of(task, ucc_schedule_pipelined_t);
    ucc_schedule_t **frags = schedule_p->frags;
    /* printf("schedule pipelined %p is complete\n", schedule_p); */
    for (i = 0; i < schedule_p->n_frags; i++) {
        schedule_p->frags[i]->super.finalize(&frags[i]->super);
    }
    return UCC_OK;
}

ucc_status_t ucc_schedule_pipelined_post(ucc_coll_task_t *task)
{
    int i, j;
    ucc_schedule_pipelined_t *schedule_p = ucc_derived_of(task, ucc_schedule_pipelined_t);
    ucc_schedule_t **frags = schedule_p->frags;

    schedule_p->super.super.super.status = UCC_OPERATION_INITIALIZED;
    schedule_p->super.n_completed_tasks = 0;
    schedule_p->n_frags_started         = 0;
    for (i = 0; i < schedule_p->n_frags; i++) {
        frags[i]->n_completed_tasks = 0;
        frags[i]->super.super.status = UCC_OPERATION_INITIALIZED;
        for (j = 0; j < frags[0]->n_tasks; j++) {
            frags[i]->tasks[j]->n_deps = frags[i]->tasks[j]->n_deps_orig;
            frags[i]->tasks[j]->n_deps_satisfied = 0;
            frags[i]->tasks[j]->super.status = UCC_OPERATION_INITIALIZED;
            if (i == 0 && schedule_p->n_frags > 1 && schedule_p->sequential) {
                frags[0]->tasks[j]->n_deps_satisfied++;
            }
        }
    }

    return ucc_schedule_start(&schedule_p->super);
}

ucc_status_t ucc_schedule_pipelined_init(ucc_base_coll_args_t *coll_args,
                                         ucc_base_team_t *team,
                                         ucc_schedule_frag_init_fn_t frag_init,
                                         ucc_schedule_frag_setup_fn_t frag_setup,
                                         int n_frags,
                                         int n_frags_total,
                                         int sequential,
                                         ucc_schedule_pipelined_t *schedule)
{
    int i,j;
    ucc_status_t status;
    ucc_schedule_t **frags;

    ucc_schedule_init(&schedule->super, &coll_args->args, team, 0);
    schedule->super.n_tasks           = n_frags_total; //TODO compute
    schedule->n_frags                 = n_frags;
    schedule->sequential                 = sequential;
    schedule->frag_setup           = frag_setup;
    schedule->next_frag_to_post    = 0;
    schedule->super.super.finalize = ucc_schedule_pipelined_finalize;
    schedule->super.super.post = ucc_schedule_pipelined_post;
    frags                             = schedule->frags;
    for (i = 0; i < n_frags; i++) {
        status = frag_init(coll_args, schedule, team, &frags[i]);
        if (UCC_OK != status) {
            ucc_error("failed to initialize fragment for pipeline");
            goto err;
        }
        frags[i]->super.super.status = UCC_OPERATION_INITIALIZED;
    }
    for (i = 0; i < n_frags; i++) {
        for (j = 0; j < frags[i]->n_tasks; j++) {
            frags[i]->tasks[j]->n_deps_orig = frags[i]->tasks[j]->n_deps;
            if (n_frags > 1 && sequential) {
                ucc_event_manager_subscribe(&frags[(i > 0) ? (i-1) : (n_frags - 1)]->tasks[j]->em,
                                            UCC_EVENT_COMPLETED, frags[i]->tasks[j],
                                            ucc_dependency_handler);
                frags[i]->tasks[j]->n_deps_orig++;
            }
        }
        ucc_event_manager_subscribe(&schedule->super.super.em, UCC_EVENT_SCHEDULE_STARTED,
                                    &frags[i]->super, ucc_frag_start_handler);
        ucc_event_manager_subscribe(&frags[i]->super.em, UCC_EVENT_COMPLETED,
                                    &schedule->super.super,
                                    (sequential ?
                                    ucc_schedule_pipelined_completed_handler_seq :
                                    ucc_schedule_pipelined_completed_handler));

        /* printf("frag %p = [ %p %p ]\n", frags[i], frags[i]->tasks[0], frags[i]->tasks[1]); */
    }
    schedule->n_frags_in_pipeline = n_frags;
    return UCC_OK;
err:
    for (i = i - 1; i >= 0; i--) {
        frags[i]->super.finalize(&frags[i]->super);
    }
    return status;
}

ucc_status_t ucc_dependency_handler(ucc_coll_task_t *parent, /* NOLINT */
                                    ucc_coll_task_t *task)
{
    task->n_deps_satisfied++;
    /* printf("task %p, n_deps %d, satisfied %d\n", */
           /* task, task->n_deps, task->n_deps_satisfied); */
    if (task->n_deps == task->n_deps_satisfied) {
        return task->post(task);
    }
    return UCC_OK;
}
