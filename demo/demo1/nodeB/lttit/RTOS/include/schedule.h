
#ifndef SCHEDULE_H
#define SCHEDULE_H


#include <stddef.h>
#include <stdint.h>
#include "rbtree.h"
#define true    1
#define false   0


//if a task is Dead, the leisure task will delete it.
#define Ready       0
#define Suspend     1
#define Dead        3
#define Delay       4


#define configSysTickClockHz			( ( unsigned long ) 72000000 )
#define configTickRateHz			( ( uint32_t ) 1000 )

#define configShieldInterPriority 191




typedef void (* TaskFunction_t)(void *);
typedef  struct TCB_t         *TaskHandle_t;

void TaskCreate(TaskFunction_t TaskCode,
                uint16_t StackDepth,
                void *Parameters,
                uint16_t period,
                uint8_t respondLine,
                uint16_t deadline,
                TaskHandle_t *self);

void TaskDelete(TaskHandle_t self);
void TaskDelay(uint16_t ticks);
uint32_t TaskEnter(void);
uint32_t TaskExit(void);
TaskHandle_t TaskFirstRespond(rb_root_handle root);

void TaskTreeAdd(TaskHandle_t self, uint8_t State);
void TaskTreeRemove(TaskHandle_t self, uint8_t State);
void DelayTreeRemove(TaskHandle_t self);

void Insert_IPC(TaskHandle_t self, rb_root_handle root);
void Remove_IPC(TaskHandle_t self);
TaskHandle_t FirstRespond_IPC(rb_root_handle root);


void SchedulerInit(void);
void SchedulerStart(void);

uint8_t CheckIPCState(TaskHandle_t taskHandle);

TaskHandle_t GetCurrentTCB(void);
uint8_t GetRespondLine(TaskHandle_t self);

uint8_t SetRespondLine(TaskHandle_t self, uint8_t respondLine);


void CheckTicks(void);

uint32_t xEnterCritical();
void xExitCritical(uint32_t xre);

#endif
