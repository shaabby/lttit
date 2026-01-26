#ifndef _HEAP_H_
#define _HEAP_H_

#include <stddef.h>
#include <stdint.h>

void *heap_malloc(size_t size);
void heap_free(void *ptr);

#endif
