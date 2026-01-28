#ifndef _HEAP_H_
#define _HEAP_H_

#include <stddef.h>
#include <stdint.h>

struct heap_stats {
    uint32_t remain_size;
    uint32_t free_size_iter;
    uint32_t max_free_block;
    uint32_t free_blocks;
};

void *heap_malloc(size_t size);
void heap_free(void *ptr);

struct heap_stats heap_get_stats(void);

#endif
