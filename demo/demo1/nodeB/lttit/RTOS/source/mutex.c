#include "mutex.h"
#include "heap.h"
#include "port.h"
#include "schedule.h"
#include "rbtree.h"

struct mutex {
    uint8_t value;
    struct rb_root wait_tree;
    TaskHandle_t owner;
};

extern uint8_t schedule_PendSV;

mutex_handle mutex_create(void)
{
    struct mutex *m;

    m = heap_malloc(sizeof(*m));
    if (!m)
        return NULL;

    *m = (struct mutex){
            .value = 1,
            .owner = NULL,
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
    uint8_t volatile pend;

    key = EnterCritical();
    cur = get_current_tcb();

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
        insert_ipc(cur, &m->wait_tree);
        task_delay(ticks);
    }

    ExitCritical(key);

    while (pend == schedule_PendSV)
        ;

    key = EnterCritical();

    if (!check_ipc_state(cur)) {
        remove_ipc(cur);
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

    key = EnterCritical();
    cur = get_current_tcb();

    if (m->wait_tree.count) {
        TaskHandle_t t = first_respond_ipc(&m->wait_tree);

        delay_tree_remove(t);
        remove_ipc(t);
        task_tree_add(t, Ready);

        if (sched_should_preempt(t, cur))
            schedule();
    }

    m->value++;

    ExitCritical(key);
    return 1;
}
