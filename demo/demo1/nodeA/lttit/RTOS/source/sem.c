#include "sem.h"
#include "heap.h"
#include "port.h"
#include "schedule.h"

struct semaphore {
    uint8_t value;
    struct rb_root wait_tree;
};

semaphore_handle semaphore_creat(uint8_t value)
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

extern uint8_t schedule_PendSV;

uint8_t semaphore_release(semaphore_handle sem)
{
    uint32_t key;
    TaskHandle_t cur;
    uint8_t prio;

    key = EnterCritical();
    cur = GetCurrentTCB();
    prio = GetRespondLine(cur);

    if (sem->wait_tree.count) {
        TaskHandle_t t = FirstRespond_IPC(&sem->wait_tree);

        DelayTreeRemove(t);
        Remove_IPC(t);
        TaskTreeAdd(t, Ready);

        if (GetRespondLine(t) < prio)
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
    cur = GetCurrentTCB();

    if (sem->value > 0) {
        sem->value--;
        ExitCritical(key);
        return 1;
    }

    if (ticks == 0) {
        ExitCritical(key);
        return 0;
    }

    pend = schedule_PendSV;

    if (ticks > 0) {
        Insert_IPC(cur, &sem->wait_tree);
        TaskDelay(ticks);
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

    sem->value--;
    ExitCritical(key);

    return 1;
}
