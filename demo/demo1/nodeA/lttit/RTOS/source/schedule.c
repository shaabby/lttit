#include "schedule.h"
#include "heap.h"
#include "port.h"
#include "hashmap.h"
#include "rbtree.h"
#include "atomic.h"
#include "macro.h"
#include "compare.h"
#include <stdio.h>

#define SCHED_DEBUG 0
#if SCHED_DEBUG
#define sched_log(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define sched_log(fmt, ...) do {} while (0)
#endif

extern uint32_t *StackInit(uint32_t *pxTopOfStack, TaskFunction_t pxCode,
                           void *pvParameters);
extern void StartFirstTask(void);
extern void ErrorHandle(void);
extern uint32_t EnterCritical(void);
extern void ExitCritical(uint32_t xReturn);

struct rb_root ReadyTree;
struct rb_root WakeTicksTree;
struct rb_root SuspendTree;
struct rb_root DeleteTree;
struct hashmap pid_map;

volatile uint32_t NowTickCount = 0;
static uint32_t next_pid = 1;

struct TCB_t {
    volatile uint32_t *pxTopOfStack;
    uint32_t pid;
    struct rb_node task_node;
    struct rb_node IPC_node;
    uint16_t period;
    uint8_t respondLine;
    uint16_t deadline;
    uint32_t EnterTime;
    uint32_t ExitTime;
    uint32_t SmoothTime;
    uint32_t stack_mem;
    uint32_t max_used_mem;
    uint32_t *pxStack;
};

__attribute__((used)) TaskHandle_t volatile schedule_currentTCB;

static inline uint32_t make_prio(uint32_t val)
{
    return val;
}

static inline uint8_t task_is_rt(TaskHandle_t t)
{
    return t->deadline != 0;
}

#define RT_WINDOW (0xFFFF >> 1)

static inline uint32_t rt_prio_value(TaskHandle_t t)
{
    return NowTickCount - (RT_WINDOW - t->deadline);
}

static inline uint32_t be_prio_value(TaskHandle_t t)
{
    return NowTickCount + t->respondLine;
}

static inline uint32_t calc_task_prio(TaskHandle_t t)
{
    if (task_is_rt(t))
        return make_prio(rt_prio_value(t));
    else
        return make_prio(be_prio_value(t));
}

TaskHandle_t GetCurrentTCB(void)
{
    return schedule_currentTCB;
}

uint32_t GetPrio(TaskHandle_t self)
{
    return self->task_node.value;
}

uint32_t reset_ready_prio(TaskHandle_t self, uint32_t prio)
{
    TaskTreeRemove(self, Ready);
    self->task_node.value = prio;
    TaskTreeAdd(self, Ready);
    return self->task_node.value;
}

uint32_t task_pid_alloc(void)
{
    return next_pid++;
}

TaskHandle_t TaskFirstRespond(rb_root_handle root)
{
    struct rb_node *n = root->first_node;
    return container_of(n, struct TCB_t, task_node);
}

TaskHandle_t FirstRespond_IPC(rb_root_handle root)
{
    struct rb_node *n = root->first_node;
    return container_of(n, struct TCB_t, IPC_node);
}

static void ReadyTreeAdd(struct rb_node *node)
{
    TaskHandle_t self = container_of(node, struct TCB_t, task_node);
    uint32_t prio = calc_task_prio(self);
    node->root = &ReadyTree;
    node->value = prio;
    rb_insert_node(&ReadyTree, node);
    sched_log("[READY] pid=%lu prio=%lu\n", self->pid, prio);
}

static void ReadyTreeRemove(struct rb_node *node)
{
    TaskHandle_t self = container_of(node, struct TCB_t, task_node);
    rb_remove_node(&ReadyTree, node);
    sched_log("[REMOVE] pid=%lu\n", self->pid);
}

static void SuspendTreeAdd(struct rb_node *node)
{
    node->root = &SuspendTree;
    rb_insert_node(&SuspendTree, node);
}

static void SuspendTreeRemove(struct rb_node *node)
{
    rb_remove_node(&SuspendTree, node);
}

void TaskTreeAdd(TaskHandle_t self, uint8_t State)
{
    uint32_t key = EnterCritical();
    struct rb_node *node = &self->task_node;
    void (*TreeAdd[])(struct rb_node *node) = {
            ReadyTreeAdd,
            SuspendTreeAdd
    };
    TreeAdd[State](node);
    ExitCritical(key);
}

void TaskTreeRemove(TaskHandle_t self, uint8_t State)
{
    uint32_t key = EnterCritical();
    struct rb_node *node = &self->task_node;
    void (*TreeRemove[])(struct rb_node *node) = {
            ReadyTreeRemove,
            SuspendTreeRemove
    };
    TreeRemove[State](node);
    ExitCritical(key);
}

void Insert_IPC(TaskHandle_t self, struct rb_root *root)
{
    uint32_t prio = calc_task_prio(self);
    self->IPC_node.root = root;
    self->IPC_node.value = prio;
    rb_insert_node(root, &self->IPC_node);
    sched_log("[IPC-ADD] pid=%lu prio=%lu\n", self->pid, prio);
}

void Remove_IPC(TaskHandle_t self)
{
    rb_remove_node(self->IPC_node.root, &self->IPC_node);
    self->IPC_node.root = NULL;
    sched_log("[IPC-DEL] pid=%lu\n", self->pid);
}

void ADTTreeInit(void)
{
    rb_root_init(&ReadyTree);
    rb_root_init(&SuspendTree);
    rb_root_init(&DeleteTree);
}

uint8_t CheckIPCState(TaskHandle_t taskHandle)
{
    return taskHandle->IPC_node.root == NULL;
}

void RecordWakeTime(uint16_t ticks)
{
    uint32_t key = EnterCritical();
    const uint32_t constTicks = NowTickCount;
    struct TCB_t *self = schedule_currentTCB;
    self->task_node.value = constTicks + ticks;
    self->task_node.root = &WakeTicksTree;
    rb_insert_node(&WakeTicksTree, &self->task_node);
    ExitCritical(key);
}

void TaskDelay(uint16_t ticks)
{
    if (ticks) {
        TaskTreeRemove(schedule_currentTCB, Ready);
        RecordWakeTime(ticks);
        schedule();
    }
}

void TreeDelayInit(void)
{
    rb_root_init(&WakeTicksTree);
}

uint32_t TaskCreate(TaskFunction_t TaskCode,
                    uint16_t StackDepth,
                    void *Parameters,
                    uint16_t period,
                    uint8_t respondLine,
                    uint16_t deadline,
                    TaskHandle_t *self)
{
    uint32_t *topStack;
    uint32_t *pxStack;
    struct TCB_t *NewTcb;
    size_t stack_mem = (size_t)StackDepth * sizeof(uintptr_t);

    pxStack = (uint32_t *)heap_malloc(stack_mem);
    NewTcb = heap_malloc(sizeof(*NewTcb));

    *self = NewTcb;
    *NewTcb = (struct TCB_t){
            .period      = period,
            .respondLine = respondLine,
            .deadline    = deadline,
            .SmoothTime  = 0,
            .stack_mem   = stack_mem,
            .pxStack     = pxStack,
            .pid         = task_pid_alloc(),
    };

    hashmap_put(&pid_map, (void *)(uintptr_t)NewTcb->pid, NewTcb);

    topStack = NewTcb->pxStack + (StackDepth - 1);
    topStack = (uint32_t *)((uint32_t)topStack &
                            ~(uint32_t)alignment_byte);

    NewTcb->pxTopOfStack = StackInit(topStack, TaskCode, Parameters);

    rb_node_init(&NewTcb->task_node);
    rb_node_init(&NewTcb->IPC_node);

    sched_log("[CREATE] pid=%lu deadline=%u respond=%u period=%u\n",
              NewTcb->pid, NewTcb->deadline, NewTcb->respondLine, NewTcb->period);

    if (NewTcb->pid != 1)
        TaskTreeAdd(NewTcb, Ready);

    return NewTcb->pid;
}

void TaskDelete(TaskHandle_t self)
{
    uint32_t key = EnterCritical();

    sched_log("[DELETE] pid=%lu\n", self->pid);

    hashmap_remove(&pid_map, (void *)(uintptr_t)self->pid);
    TaskTreeRemove(self, Ready);
    self->task_node.root = &DeleteTree;
    rb_insert_node(&DeleteTree, &self->task_node);

    ExitCritical(key);
    schedule();
}

uint32_t TaskEnter(void)
{
    struct TCB_t *self = schedule_currentTCB;
    self->EnterTime = NowTickCount;
    sched_log("[ENTER] pid=%lu time=%lu\n", self->pid, self->EnterTime);
    return self->EnterTime;
}

uint32_t TaskExit(void)
{
    struct TCB_t *self = schedule_currentTCB;
    uint32_t newPeriod;

    self->ExitTime = NowTickCount;
    newPeriod = self->ExitTime - self->EnterTime;

    if (self->SmoothTime != 0)
        self->SmoothTime = (self->SmoothTime * 7 + newPeriod) >> 3;
    else
        self->SmoothTime = newPeriod;

    if (self->deadline && newPeriod >= self->deadline)
        ErrorHandle();

    sched_log("[EXIT] pid=%lu exec=%lu smooth=%lu\n",
              self->pid, newPeriod, self->SmoothTime);

    TaskDelay(self->period);

    return newPeriod;
}

void TaskFree(void)
{
    uint32_t key = EnterCritical();

    if (DeleteTree.count) {
        struct rb_node *n = rb_last(&DeleteTree);
        TaskHandle_t self =
                container_of(n, struct TCB_t, task_node);

        rb_remove_node(&DeleteTree, &self->task_node);
        ExitCritical(key);

        sched_log("[FREE] pid=%lu\n", self->pid);

        heap_free(self->pxStack);
        heap_free(self);
    } else {
        ExitCritical(key);
    }
}

TaskHandle_t leisureTcb;
uint32_t leisureCount;

uint8_t is_leisure(void)
{
    return schedule_currentTCB == leisureTcb;
}

void leisureTask(void)
{
    for (;;) {
        leisureCount++;
        TaskFree();
    }
}

void LeisureTaskCreat(void)
{
    TaskCreate((TaskFunction_t)leisureTask,
               64,
               NULL,
               0,
               0,
               0,
               &leisureTcb);
}

void SchedulerInit(void)
{
    ADTTreeInit();
    hashmap_init(&pid_map, TASK_COUNT, HASHMAP_KEY_INT);
    TreeDelayInit();
    LeisureTaskCreat();
}

void rtos_stack_used(TaskHandle_t tcb)
{
    uint8_t *stack_end = (uint8_t *)tcb->pxStack + tcb->stack_mem;
    size_t used = (size_t)(stack_end - (uint8_t *)tcb->pxTopOfStack);
    if (used > tcb->max_used_mem)
        tcb->max_used_mem = used;
}

uint8_t volatile schedule_PendSV;

void TaskSwitchContext(void)
{
    TaskHandle_t old = schedule_currentTCB;

    schedule_PendSV++;

    if (schedule_currentTCB)
        rtos_stack_used(schedule_currentTCB);

    if (ReadyTree.count == 0)
        schedule_currentTCB = leisureTcb;
    else
        schedule_currentTCB = TaskFirstRespond(&ReadyTree);

    sched_log("[CTX] %lu -> %lu\n",
              old ? old->pid : 0,
              schedule_currentTCB ? schedule_currentTCB->pid : 0);
}

void SchedulerStart(void)
{
    TaskSwitchContext();
    StartFirstTask();
}

void DelayTreeRemove(TaskHandle_t self)
{
    rb_remove_node(&WakeTicksTree, &self->task_node);
}

uint8_t SusPend = 1;

void CheckTicks(void)
{
    struct rb_node *n;

    NowTickCount++;

    if (SusPend) {
        uint32_t key = EnterCritical();

        while ((n = WakeTicksTree.first_node) &&
               compare_before_eq(n->value, NowTickCount)) {
            TaskHandle_t self =
                    container_of(n, struct TCB_t, task_node);

            DelayTreeRemove(self);
            TaskTreeAdd(self, Ready);

            sched_log("[WAKE] pid=%lu at tick=%lu\n", self->pid, NowTickCount);

            if (schedule_currentTCB == leisureTcb ||
                compare_before_eq(self->task_node.value,
                                  schedule_currentTCB->task_node.value))
                schedule();
        }

        ExitCritical(key);
    }
}

uint8_t get_task_state(TaskHandle_t tcb)
{
    if (tcb == schedule_currentTCB)
        return RUNNING;

    if (tcb->task_node.root == &ReadyTree)
        return Ready;

    if (tcb->task_node.root == &WakeTicksTree)
        return OS_Delay;

    if (tcb->task_node.root == &SuspendTree)
        return Suspend;

    if (tcb->task_node.root == &DeleteTree)
        return Dead;

    return Suspend;
}

int rtos_get_task_info(uint32_t pid, struct task_info *out)
{
    uint32_t key = EnterCritical();
    TaskHandle_t tcb = hashmap_get(&pid_map, (void *)(uintptr_t)pid);
    if (!tcb) {
        ExitCritical(key);
        return -1;
    }

    out->pid = tcb->pid;
    out->stack_watermark = tcb->max_used_mem;
    out->period = tcb->period;
    out->deadline = tcb->deadline;
    out->state = get_task_state(tcb);
    ExitCritical(key);

    return 0;
}
