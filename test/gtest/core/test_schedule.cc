#include <common/test.h>
#include <common/test_ucc.h>
extern "C" {
#include "schedule/ucc_schedule_pipelined.h"
#include "core/ucc_context.h"
#include "core/ucc_progress_queue.h"
}

class test_schedule : public ucc::test {
};
#if 0
#define CTX() (UccJob::getStaticJob()->procs.begin()->get()->ctx_h)

static ucc_status_t _ucc_task_post(ucc_coll_task_t *task)
{
    ucc_progress_enqueue(CTX()->pq, task);
    return UCC_OK;
}

static ucc_status_t __task_finalize(ucc_coll_task_t *task)
{
    std::cout << "finalizing " << task << std::endl;
    memset(task, 0, sizeof(*task));
    delete task;
    return UCC_OK;
}

static ucc_status_t __task_progress(ucc_coll_task_t *task)
{
    printf("completing task %p\n", task);
    task->super.status = UCC_OK;
    return UCC_OK;
}

#define N_TASKS 4

ucc_status_t __test_init_frag(ucc_base_coll_args_t *coll_args,
                              ucc_base_team_t      *team,
                              ucc_schedule_frag_t   **frag_p)
{
    ucc_schedule_frag_t *frag = new ucc_schedule_frag_t;
    EXPECT_EQ(UCC_OK, ucc_schedule_frag_init(frag, CTX()));
    std::vector<ucc_coll_task_t*> tasks;
    for (auto i = 0; i < N_TASKS; i++) {
        ucc_coll_task_t *t = new ucc_coll_task_t;
        ucc_coll_task_init_dependent(t, 1);
        t->finalize = __task_finalize;
        t->progress = __task_progress;
        t->post  = _ucc_task_post;
        tasks.push_back(t);
        ucc_schedule_frag_add_task(frag, t);
        if (i == 0) {
            ucc_event_manager_subscribe(&frag->super.super.em, UCC_EVENT_SCHEDULE_STARTED, t);
        } else if (i < 3) {
            ucc_event_manager_subscribe(&tasks[0]->em, UCC_EVENT_COMPLETED, t);
        } else {
            ucc_event_manager_subscribe(&tasks[i-1]->em, UCC_EVENT_COMPLETED, t);
        }
    }
    printf("frag %p = [ ", frag);
    for (int i = 0; i < N_TASKS; i++) {
        printf("%p ", tasks[i]);
    }
    printf("]\n");
    *frag_p = frag;
    return UCC_OK;
}

ucc_status_t __test_fini_frag(ucc_schedule_frag_t *frag) {
    delete frag;
    return UCC_OK;
}

ucc_status_t __test_setup_frag(ucc_schedule_pipelined_t *schedule_p,
                               ucc_schedule_frag_t *frag, int frag_num)
{
    printf ("setting up fragment %p frag_num %d\n", frag, frag_num);
    return UCC_OK;
}

UCC_TEST_F(test_schedule, basic)
{
    ucc_schedule_pipelined_t *schedule_p;
    ucc_schedule_pipelined_init(NULL, NULL, __test_init_frag, __test_fini_frag,
                                __test_setup_frag, 1, 2, &schedule_p);

    ASSERT_EQ(UCC_OK, ucc_collective_post(&schedule_p->super.super.super));
    while (UCC_OK != schedule_p->super.super.super.status) {
        ASSERT_EQ(UCC_OK, ucc_context_progress(CTX()));
    }
    ucc_collective_finalize(&schedule_p->super.super.super);
}
#endif
