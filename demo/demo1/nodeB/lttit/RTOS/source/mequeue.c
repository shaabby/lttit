#include <string.h>
#include "mequeue.h"
#include "heap.h"
#include "port.h"
#include "rbtree.h"

struct queue_struct {
    uint8_t *start;
    uint8_t *end;
    uint8_t *read;
    uint8_t *write;
    uint8_t msg_num;
    struct rb_root send_tree;
    struct rb_root recv_tree;
    uint32_t node_size;
    uint32_t node_num;
};

extern uint8_t schedule_PendSV;

#define get_top_tcb_index FindHighestPriority

static void write_to_queue(struct queue_struct *q, uint32_t *buf,
                           uint8_t cur_prio)
{
    memcpy(q->write, buf, (size_t)q->node_size);
    q->write += q->node_size;

    if (q->write >= q->end)
        q->write = q->start;

    if (q->recv_tree.count) {
        TaskHandle_t t = FirstRespond_IPC(&q->recv_tree);

        DelayTreeRemove(t);
        Remove_IPC(t);
        TaskTreeAdd(t, Ready);

        if (GetRespondLine(t) > cur_prio)
            schedule();
    }

    q->msg_num++;
}

static void extract_from_queue(struct queue_struct *q, uint32_t *buf,
                               uint8_t cur_prio)
{
    q->read += q->node_size;

    if (q->read >= q->end)
        q->read = q->start;

    memcpy(buf, q->read, (size_t)q->node_size);

    if (q->send_tree.count) {
        TaskHandle_t t = FirstRespond_IPC(&q->send_tree);

        DelayTreeRemove(t);
        Remove_IPC(t);
        TaskTreeAdd(t, Ready);

        if (GetRespondLine(t) > cur_prio)
            schedule();
    }

    q->msg_num--;
}

struct queue_struct *queue_create(uint32_t len, uint32_t size)
{
    size_t qsize = (size_t)len * size;
    struct queue_struct *q;
    uint8_t *msg_start;

    q = heap_malloc(sizeof(*q) + qsize);
    if (!q)
        return NULL;

    msg_start = (uint8_t *)q + sizeof(*q);

    *q = (struct queue_struct){
            .start = msg_start,
            .end = msg_start + qsize,
            .read = msg_start + (len - 1) * size,
            .write = msg_start,
            .msg_num = 0,
            .node_num = len,
            .node_size = size,
    };

    rb_root_init(&q->send_tree);
    rb_root_init(&q->recv_tree);

    return q;
}

void queue_delete(struct queue_struct *q)
{
    heap_free(q);
}

uint8_t queue_send(struct queue_struct *q, uint32_t *buf, uint32_t ticks)
{
    uint32_t key;
    TaskHandle_t cur;
    uint8_t prio;
    uint8_t volatile pend;

    key = EnterCritical();
    cur = GetCurrentTCB();
    prio = GetRespondLine(cur);

    if (q->msg_num < q->node_num) {
        write_to_queue(q, buf, prio);
        ExitCritical(key);
        return 1;
    }

    if (q->msg_num == q->node_num) {
        if (!ticks) {
            ExitCritical(key);
            return 0;
        }
    } else {
        ExitCritical(key);
        return 0;
    }

    pend = schedule_PendSV;

    if (ticks > 0) {
        Insert_IPC(cur, &q->send_tree);
        TaskDelay(ticks);
    }

    ExitCritical(key);

    while (pend == schedule_PendSV)
        ;

    key = EnterCritical();

    if (!CheckIPCState(cur)) {
        ExitCritical(key);
        return 0;
    }

    write_to_queue(q, buf, prio);
    ExitCritical(key);

    return 1;
}

uint8_t queue_receive(struct queue_struct *q, uint32_t *buf, uint32_t ticks)
{
    uint32_t key;
    TaskHandle_t cur;
    uint8_t prio;
    uint8_t volatile pend;

    key = EnterCritical();
    cur = GetCurrentTCB();
    prio = GetRespondLine(cur);

    if (q->msg_num > 0) {
        extract_from_queue(q, buf, prio);
        ExitCritical(key);
        return 1;
    }

    if (q->msg_num == 0) {
        if (!ticks) {
            ExitCritical(key);
            return 0;
        }
    }

    pend = schedule_PendSV;

    if (ticks > 0) {
        Insert_IPC(cur, &q->recv_tree);
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

    extract_from_queue(q, buf, prio);
    ExitCritical(key);

    return 1;
}
