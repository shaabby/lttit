#include "schedule.h"
#include "heap.h"
#include "port.h"
#include "hashmap.h"
#include "rbtree.h"
#include "atomic.h"
#include "macro.h"


extern uint32_t *StackInit(uint32_t *pxTopOfStack, TaskFunction_t pxCode,
                           void *pvParameters);
extern void StartFirstTask(void);
extern void ErrorHandle(void);
extern uint32_t EnterCritical(void);
extern void ExitCritical(uint32_t xReturn);

struct rb_root ReadyTree;
struct rb_root OneDelayTree;
struct rb_root TwoDelayTree;
struct rb_root *WakeTicksTree;
struct rb_root *OverWakeTicksTree;
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

TaskHandle_t GetCurrentTCB(void)
{
    return schedule_currentTCB;
}

uint8_t GetRespondLine(TaskHandle_t self)
{
    return self->respondLine;
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

uint8_t SetRespondLine(TaskHandle_t self, uint8_t respondLine)
{
    return (uint8_t)atomic_set_return(respondLine,
                                      (uint32_t *)&self->respondLine);
}

static void ReadyTreeAdd(struct rb_node *node)
{
    TaskHandle_t self = container_of(node, struct TCB_t, task_node);
    node->root = &ReadyTree;
    node->value = NowTickCount + self->respondLine;
    rb_insert_node(&ReadyTree, node);
}

static void ReadyTreeRemove(struct rb_node *node)
{
    rb_remove_node(&ReadyTree, node);
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
    self->IPC_node.root = root;
    self->IPC_node.value = NowTickCount + self->respondLine;
    rb_insert_node(root, &self->IPC_node);
}

void Remove_IPC(TaskHandle_t self)
{
    rb_remove_node(self->IPC_node.root, &self->IPC_node);
    self->IPC_node.root = NULL;
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

void rtos_stack_used(TaskHandle_t tcb)
{
    uint8_t *stack_end = (uint8_t *)tcb->pxStack + tcb->stack_mem;
    size_t used = (size_t)(stack_end - (uint8_t *)tcb->pxTopOfStack);
    if (used > tcb->max_used_mem) {
        tcb->max_used_mem = used;
    }
}

uint8_t volatile schedule_PendSV;
void TaskSwitchContext(void)
{
    schedule_PendSV++;
    if (schedule_currentTCB) {
        rtos_stack_used(schedule_currentTCB);
    }
    schedule_currentTCB = TaskFirstRespond(&ReadyTree);
}

void RecordWakeTime(uint16_t ticks)
{
    const uint32_t constTicks = NowTickCount;
    struct TCB_t *self = schedule_currentTCB;

    self->task_node.value = constTicks + ticks;

    if (self->task_node.value < constTicks) {
        self->task_node.root = OverWakeTicksTree;
        rb_insert_node(OverWakeTicksTree, &self->task_node);
    } else {
        self->task_node.root = WakeTicksTree;
        rb_insert_node(WakeTicksTree, &self->task_node);
    }
}

void TaskDelay(uint16_t ticks)
{
    if (ticks) {
        TaskTreeRemove(schedule_currentTCB, Ready);
        RecordWakeTime(ticks);
        schedule();
    }
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
            .period = period,
            .respondLine = respondLine,
            .deadline = deadline,
            .SmoothTime = 0,
            .stack_mem = stack_mem,
            .pxStack = pxStack,
            .pid = task_pid_alloc(),
    };
    hashmap_put(&pid_map, (void *)(uintptr_t)NewTcb->pid, NewTcb);
    topStack = NewTcb->pxStack + (StackDepth - 1);
    topStack = (uint32_t *)((uint32_t)topStack &
                            ~(uint32_t)alignment_byte);

    NewTcb->pxTopOfStack = StackInit(topStack, TaskCode, Parameters);

    rb_node_init(&NewTcb->task_node);
    rb_node_init(&NewTcb->IPC_node);

    TaskTreeAdd(NewTcb, Ready);
    return NewTcb->pid;
}

void TaskDelete(TaskHandle_t self)
{
    hashmap_remove(&pid_map, (void *)(uintptr_t)self->pid);
    TaskTreeRemove(self, Ready);
    self->task_node.root = &DeleteTree;
    rb_insert_node(&DeleteTree, &self->task_node);
    schedule();
}

uint32_t TaskEnter(void)
{
    struct TCB_t *self = schedule_currentTCB;

    self->EnterTime = NowTickCount;
    return self->EnterTime;
}

uint32_t TaskExit(void)
{
    struct TCB_t *self = schedule_currentTCB;
    uint32_t newPeriod;

    self->ExitTime = NowTickCount;
    newPeriod = self->ExitTime - self->EnterTime;

    if (self->SmoothTime != 0)
        self->SmoothTime =
                (self->SmoothTime - (self->SmoothTime >> 3)) +
                (newPeriod << 13);
    else
        self->SmoothTime = newPeriod << 16;

    if (newPeriod >= self->deadline)
        ErrorHandle();

    TaskDelay(self->period);

    return newPeriod;
}

void TreeDelayInit(void)
{
    rb_root_init(&OneDelayTree);
    rb_root_init(&TwoDelayTree);

    WakeTicksTree = &OneDelayTree;
    OverWakeTicksTree = &TwoDelayTree;
}

void TaskFree(void)
{
    if (DeleteTree.count) {
        struct rb_node *n = rb_last(&DeleteTree);
        TaskHandle_t self =
        container_of(n, struct TCB_t, task_node);

        rb_remove_node(&DeleteTree, &self->task_node);
        heap_free(self->pxStack);
        heap_free(self);
    }
}

TaskHandle_t leisureTcb;
uint32_t leisureCount;

void leisureTask(void)
{
    for (;;) {
        leisureCount++;
        TaskFree();
    }
}

uint32_t MaxRespondLine = (uint32_t)~0;

void LeisureTaskCreat(void)
{
    TaskCreate((TaskFunction_t)leisureTask,
               128,
               NULL,
               0,
               MaxRespondLine,
               MaxRespondLine,
               &leisureTcb);

    leisureTcb->task_node.value = MaxRespondLine;
}

void SchedulerInit(void)
{
    ADTTreeInit();
    hashmap_init(&pid_map, TASK_COUNT, HASHMAP_KEY_INT);
    TreeDelayInit();
    LeisureTaskCreat();
}

void SchedulerStart(void)
{
    TaskSwitchContext();
    StartFirstTask();
}

void DelayTreeRemove(TaskHandle_t self)
{
    rb_remove_node(WakeTicksTree, &self->task_node);
}

uint8_t SusPend = 1;

void CheckTicks(void)
{
    struct rb_node *n;

    NowTickCount++;

    if (SusPend) {
        if (NowTickCount == 0) {
            struct rb_root *tmp = WakeTicksTree;

            WakeTicksTree = OverWakeTicksTree;
            OverWakeTicksTree = tmp;
        }

        while ((n = WakeTicksTree->first_node) &&
               n->value <= NowTickCount) {
            TaskHandle_t self =
            container_of(n, struct TCB_t, task_node);

            DelayTreeRemove(self);
            TaskTreeAdd(self, Ready);

            if (self->task_node.value <=
                schedule_currentTCB->task_node.value)
                schedule();
        }
    }
}


uint8_t get_task_state(TaskHandle_t tcb)
{
    if (tcb == schedule_currentTCB)
        return RUNNING;

    if (tcb->task_node.root == &ReadyTree)
        return Ready;

    if (tcb->task_node.root == WakeTicksTree ||
        tcb->task_node.root == OverWakeTicksTree)
        return Delay;

    if (tcb->task_node.root == &SuspendTree)
        return Suspend;

    if (tcb->task_node.root == &DeleteTree)
        return Dead;

    return Suspend;
}

int rtos_get_task_info(uint32_t pid, struct task_info *out)
{
    TaskHandle_t tcb = hashmap_get(&pid_map, (void *)(uintptr_t)pid);
    if (!tcb)
        return -1;

    out->pid = tcb->pid;
    out->stack_watermark = tcb->max_used_mem;
    out->period = tcb->period;
    out->deadline = tcb->deadline;
    out->state = get_task_state(tcb);

    return 0;
}
