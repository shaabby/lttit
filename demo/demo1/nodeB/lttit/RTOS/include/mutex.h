#ifndef _MUTEX_H_
#define _MUTEX_H_

#include <stdint.h>
#include "rbtree.h"
#include "schedule.h"

struct mutex;
typedef struct mutex *mutex_handle;

mutex_handle mutex_creat(void);
void mutex_delete(mutex_handle m);
uint8_t mutex_lock(mutex_handle m, uint32_t ticks);
uint8_t mutex_unlock(mutex_handle m);

#endif
