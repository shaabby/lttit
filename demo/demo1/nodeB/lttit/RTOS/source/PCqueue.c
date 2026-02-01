#include "PCqueue.h"
#include "heap.h"
#include "schedule.h"

struct pc_queue {
    uint8_t in;
    uint8_t out;
    uint8_t size;
    semaphore_handle item;
    semaphore_handle space;
    int buf[];
};

pc_queue_handle pc_queue_create(uint8_t buffer_size)
{
    struct pc_queue *q;

    q = heap_malloc(sizeof(*q) + sizeof(int) * buffer_size);
    if (!q)
        return NULL;

    q->in   = 0;
    q->out  = 0;
    q->size = buffer_size;

    q->item  = semaphore_create(0);
    q->space = semaphore_create(buffer_size);

    if (!q->item || !q->space) {
        if (q->item)
            semaphore_delete(q->item);
        if (q->space)
            semaphore_delete(q->space);
        heap_free(q);
        return NULL;
    }

    return q;
}

void pc_queue_delete(pc_queue_handle q)
{
    if (!q)
        return;

    semaphore_delete(q->item);
    semaphore_delete(q->space);
    heap_free(q);
}

uint8_t pc_queue_send(pc_queue_handle q, int value, uint32_t ticks)
{
    if (!q)
        return false;

    if (!semaphore_take(q->space, ticks))
        return false;

    scheduler_lock();

    q->buf[q->in] = value;
    q->in = (uint8_t)((q->in + 1) % q->size);

    scheduler_unlock();

    semaphore_release(q->item);

    return true;
}

uint8_t pc_queue_recv(pc_queue_handle q, int *out, uint32_t ticks)
{
    if (!q || !out)
        return false;

    if (!semaphore_take(q->item, ticks))
        return false;

    scheduler_lock();

    *out = q->buf[q->out];
    q->out = (uint8_t)((q->out + 1) % q->size);

    scheduler_unlock();

    semaphore_release(q->space);

    return true;
}
