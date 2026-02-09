#ifndef _PCQUEUE_H_
#define _PCQUEUE_H_

#include <stdint.h>
#include "sem.h"
#include "mutex.h"

struct pc_queue;
typedef struct pc_queue *pc_queue_handle;
pc_queue_handle pc_queue_create(uint8_t buffer_size);

void pc_queue_delete(pc_queue_handle q);
uint8_t pc_queue_send(pc_queue_handle q, int value, uint32_t ticks);
uint8_t pc_queue_recv(pc_queue_handle q, int *out, uint32_t ticks);

#endif
