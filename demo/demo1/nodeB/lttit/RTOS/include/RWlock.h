#ifndef _RWLOCK_H_
#define _RWLOCK_H_

#include <stdint.h>

#define MAX_WAIT_TICKS 0xFFFF

struct rwlock;
typedef struct rwlock *rwlock_handle;

rwlock_handle rwlock_creat(void);
void read_acquire(rwlock_handle lock);
void read_release(rwlock_handle lock);
void write_acquire(rwlock_handle lock);
void write_release(rwlock_handle lock);
void rwlock_delete(rwlock_handle lock);

#endif
