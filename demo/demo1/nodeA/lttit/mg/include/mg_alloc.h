#ifndef _MG_ALLOC_H
#define _MG_ALLOC_H

#include <stddef.h>
#include <stdint.h>

struct mg_region;
typedef struct mg_region *mg_region_handle;

mg_region_handle mg_region_create(uint8_t max_class_bits);
void mg_region_destroy(mg_region_handle r);

void *mg_region_alloc(mg_region_handle r, size_t size);
void mg_region_reset(mg_region_handle r);

#endif
