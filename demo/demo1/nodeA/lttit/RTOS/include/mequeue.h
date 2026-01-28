#ifndef _MEQUEUE_H_
#define _MEQUEUE_H_

#include <stdint.h>

struct queue_struct;
typedef struct queue_struct *queue_handle;

struct queue_struct *queue_create(uint32_t len, uint32_t size);
void queue_delete(queue_handle q);
uint8_t queue_send(queue_handle q, uint32_t *buf, uint32_t ticks);
uint8_t queue_receive(queue_handle q, uint32_t *buf, uint32_t ticks);

#endif
