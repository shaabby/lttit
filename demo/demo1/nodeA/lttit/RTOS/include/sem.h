#ifndef _SEM_H_
#define _SEM_H_

#include <stdint.h>

struct semaphore;
typedef struct semaphore *semaphore_handle;

semaphore_handle semaphore_create(uint8_t value);
void semaphore_delete(semaphore_handle sem);
uint8_t semaphore_release(semaphore_handle sem);
uint8_t semaphore_take(semaphore_handle sem, uint32_t ticks);

#endif
