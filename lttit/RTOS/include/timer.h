#ifndef _TIMER_H_
#define _TIMER_H_

#include <stdint.h>
#include "rbtree.h"

#define run  1
#define stop 0

typedef void (*TimerFunction_t)(void *);
struct timer_obj;
typedef struct timer_obj *TimerHandle;

void timer_init(void);

TimerHandle timer_create(TimerFunction_t cb,
                         uint32_t period,
                         uint8_t flag);

void timer_delete(TimerHandle t);

void timer_tick(void);

#endif
