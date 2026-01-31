#include "sem.h"
#include "heap.h"
#include "port.h"
#include "schedule.h"
#include "rbtree.h"

struct semaphore {
    uint8_t value;
    struct rb_root wait_tree;
};

extern uint8_t schedule_PendSV;

semaphore_handle semaphore_create(uint8_t value)
{
    struct semaphore *sem;

    sem = heap_malloc(sizeof(*sem));
    if (!sem)
        return NULL;

    sem->value = value;
    rb_root_init(&sem->wait_tree);

    return sem;
}

void semaphore_delete(semaphore_handle sem)
{
    heap_free(sem);
}

uint8_t semaphore_release(semaphore_handle sem)
{
    uint32_t key;
    TaskHandle_t cur;

    key = EnterCritical();
    cur = get_current_tcb();

    if (sem->wait_tree.count) {
        TaskHandle_t t = first_respond_ipc(&sem->wait_tree);

        delay_tree_remove(t);
        remove_ipc(t);
        task_tree_add(t, Ready);

        if (sched_should_preempt(t, cur))
            schedule();
    }

    sem->value++;

    ExitCritical(key);
    return 1;
}

uint8_t semaphore_take(semaphore_handle sem, uint32_t ticks)
{
    uint32_t key;
    TaskHandle_t cur;
    uint8_t volatile pend;

    key = EnterCritical();
    cur = get_current_tcb();
    pend = schedule_PendSV;

    if (sem->value > 0) {
        sem->value--;
        ExitCritical(key);
        return 1;
    }

    if (ticks == 0) {
        ExitCritical(key);
        return 0;
    }

    insert_ipc(cur, &sem->wait_tree);

    if (ticks > 0)
        task_delay(ticks);

    ExitCritical(key);

    while (pend == schedule_PendSV)
        ;

    key = EnterCritical();

    if (!check_ipc_state(cur)) {
        remove_ipc(cur);
        ExitCritical(key);
        return 0;
    }

    if (sem->value > 0)
        sem->value--;
    else {
        ExitCritical(key);
        return 0;
    }

    ExitCritical(key);
    return 1;
}
