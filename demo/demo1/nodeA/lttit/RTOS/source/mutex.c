#include "mutex.h"
#include "heap.h"
#include "port.h"
#include "schedule.h"
#include "compare.h"

struct mutex {
    uint8_t value;
    struct rb_root wait_tree;
    TaskHandle_t owner;
};

extern uint8_t schedule_PendSV;

mutex_handle mutex_creat(void)
{
    struct mutex *m;

    m = heap_malloc(sizeof(*m));
    if (!m)
        return NULL;

    *m = (struct mutex){
            .value = 1,
            .owner = NULL
    };

    rb_root_init(&m->wait_tree);

    return m;
}

void mutex_delete(mutex_handle m)
{
    heap_free(m);
}

uint8_t mutex_lock(mutex_handle m, uint32_t ticks)
{
    uint32_t key;
    TaskHandle_t cur;
    uint32_t prio;
    uint8_t volatile pend;

    key = EnterCritical();
    cur = GetCurrentTCB();
    prio = GetPrio(cur);

    if (m->value > 0) {
        m->owner = cur;
        m->value--;
        ExitCritical(key);
        return 1;
    }

    if (ticks == 0) {
        ExitCritical(key);
        return 0;
    }

    pend = schedule_PendSV;

    if (ticks > 0) {
        Insert_IPC(cur, &m->wait_tree);
        TaskDelay(ticks);

        if (compare_after(GetPrio(m->owner), prio))
            reset_ready_prio(m->owner, prio);
    }

    ExitCritical(key);

    while (pend == schedule_PendSV)
        ;

    key = EnterCritical();

    if (!CheckIPCState(cur)) {
        Remove_IPC(cur);
        ExitCritical(key);
        return 0;
    }

    m->owner = cur;
    m->value--;

    ExitCritical(key);
    return 1;
}

uint8_t mutex_unlock(mutex_handle m)
{
    uint32_t key;
    TaskHandle_t cur;
    uint32_t cur_prio;

    key = EnterCritical();
    cur = GetCurrentTCB();
    cur_prio = GetPrio(cur);

    if (m->wait_tree.count) {
        TaskHandle_t t = FirstRespond_IPC(&m->wait_tree);

        DelayTreeRemove(t);
        Remove_IPC(t);
        TaskTreeAdd(t, Ready);

        if (is_leisure() || compare_before(GetPrio(t), cur_prio))
            schedule();
    }

    m->value++;

    ExitCritical(key);
    return 1;
}
