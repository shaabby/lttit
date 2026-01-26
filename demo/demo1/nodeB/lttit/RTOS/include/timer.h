#ifndef _TIMER_H_
#define _TIMER_H_

#include <stdint.h>
#include "schedule.h"
#include "rbtree.h"

#define run  1
#define stop 0

typedef void (*TimerFunction_t)(void *);
struct timer_obj;
typedef struct timer_obj *TimerHandle;

TaskHandle_t TimerInit(uint16_t stack, uint16_t period,
                       uint8_t respond_line, uint32_t deadline,
                       uint8_t check_period);

TimerHandle TimerCreat(TimerFunction_t cb, uint32_t period, uint8_t flag);

#endif
