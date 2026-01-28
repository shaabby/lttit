#include "mutex.h"
#include "heap.h"
#include "port.h"
#include "schedule.h"

struct mutex {
    uint8_t value;
    uint32_t original_priority;
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
            .original_priority = 0,
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
    uint8_t prio;
    uint8_t volatile pend;

    key = EnterCritical();
    cur = GetCurrentTCB();
    prio = GetRespondLine(cur);

    if (m->value > 0) {
        m->original_priority = prio;
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

        uint8_t owner_prio = GetRespondLine(m->owner);
        if (owner_prio < prio)
            SetRespondLine(m->owner, prio);
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

    m->original_priority = prio;
    m->owner = cur;
    m->value--;

    ExitCritical(key);
    return 1;
}

uint8_t mutex_unlock(mutex_handle m)
{
    uint32_t key;
    TaskHandle_t cur;
    uint8_t owner_prio;
    uint8_t cur_prio;

    key = EnterCritical();
    cur = GetCurrentTCB();

    owner_prio = GetRespondLine(m->owner);
    cur_prio = GetRespondLine(cur);

    if (m->wait_tree.count) {
        TaskHandle_t t = FirstRespond_IPC(&m->wait_tree);

        DelayTreeRemove(t);
        Remove_IPC(t);
        TaskTreeAdd(t, Ready);

        if (GetRespondLine(t) > cur_prio)
            schedule();
    }

    if (owner_prio != m->original_priority)
        SetRespondLine(m->owner, m->original_priority);

    m->value++;

    ExitCritical(key);
    return 1;
}
