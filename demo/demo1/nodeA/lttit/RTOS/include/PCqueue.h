#ifndef _PCQUEUE_H_
#define _PCQUEUE_H_

#include <stdint.h>
#include "sem.h"

#define MAX_WAIT_TICKS 0xFFFF

struct oo_buffer;
struct mo_buffer;
struct mm_buffer;

typedef struct oo_buffer *oo_buffer_handle;
typedef struct mo_buffer *mo_buffer_handle;
typedef struct mm_buffer *mm_buffer_handle;

oo_buffer_handle Oo_buffer_creat(uint8_t buffer_size);
void Oo_insert(oo_buffer_handle b, int object);
int Oo_remove(oo_buffer_handle b);
void Oo_buffer_delete(oo_buffer_handle b);

mo_buffer_handle Mo_buffer_creat(uint8_t buffer_size);
void Mo_insert(mo_buffer_handle b, int object);
int Mo_remove(mo_buffer_handle b);
void Mo_buffer_delete(mo_buffer_handle b);

mm_buffer_handle Mm_buffer_creat(uint8_t buffer_size);
void Mm_insert(mm_buffer_handle b, int object);
int Mm_remove(mm_buffer_handle b);
void Mm_buffer_delete(mm_buffer_handle b);

#endif
