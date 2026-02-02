#include "rpc_port.h"
#include "sem.h"
#include "schedule.h"
#include <stdlib.h>

struct rpc_waiter *rpc_waiter_create(void)
{
    struct rpc_waiter *w = malloc(sizeof(struct rpc_waiter));
    if (!w)
        return NULL;

    w->sem = semaphore_create(0);
    w->task = get_current_tcb();
    return w;
}

void rpc_waiter_destroy(struct rpc_waiter *w)
{
    if (!w)
        return;
    semaphore_delete(w->sem);
    free(w);
}

int rpc_waiter_wait(struct rpc_waiter *w, uint32_t timeout_ms)
{
    uint32_t ticks = timeout_ms / 1;
    rtos_task_set_waiting_obj(w->task, w);
    uint8_t ok = semaphore_take(w->sem, ticks);
    rtos_task_set_waiting_obj(w->task, NULL);
    return ok ? 0 : -1;
}

void rpc_waiter_wake(struct rpc_waiter *w)
{
    semaphore_release(w->sem);
}

uint32_t rpc_now_ms(void)
{
    return rtos_now_time();
}
