#ifndef _HEAP_H_
#define _HEAP_H_

#include <stddef.h>
#include <stdint.h>

#define HEAP_DEBUG 0

struct heap_stats {
    size_t remain_size;
    size_t free_size_iter;
    size_t max_free_block;
    size_t free_blocks;
};

void   heap_init(void);
void  *heap_malloc(size_t size);
void   heap_free(void *ptr);
struct heap_stats heap_get_stats(void);
void   heap_debug_dump_leaks(void);


#endif
