#include "mg_alloc.h"
#include "membit.h"
#include "heap.h"
#include "math.h"

#define MG_MAX_CLASS_BITS        16
#define MG_MIN_CLASS_BYTES       4

#define MG_POOL_BLOCKS_SMALL     8
#define MG_POOL_BLOCKS_MEDIUM    2
#define MG_POOL_BLOCKS_LARGE     1

#define MG_BLOCK_THRESHOLD_SMALL 64
#define MG_BLOCK_THRESHOLD_MED   128

struct mg_pool_node {
    membit_pool_t pool;
    struct mg_pool_node *next;
};

struct mg_class {
    struct mg_pool_node *pools;
    size_t block_size;
};

struct mg_block_header {
    membit_pool_t pool;
};

struct mg_region {
    struct mg_class classes[MG_MAX_CLASS_BITS + 1];
    uint8_t max_bits;
};

static inline uint8_t mg_pool_block_count(size_t block_size)
{
    if (block_size <= MG_BLOCK_THRESHOLD_SMALL)
        return MG_POOL_BLOCKS_SMALL;
    if (block_size <= MG_BLOCK_THRESHOLD_MED)
        return MG_POOL_BLOCKS_MEDIUM;
    return MG_POOL_BLOCKS_LARGE;
}

static struct mg_pool_node *mg_pool_node_new(size_t block_size)
{
    uint8_t count = mg_pool_block_count(block_size);
    membit_pool_t p = membit_create((uint16_t)block_size, count);
    if (!p)
        return NULL;

    struct mg_pool_node *n = heap_malloc(sizeof(*n));
    if (!n) {
        membit_destroy(p);
        return NULL;
    }

    n->pool = p;
    n->next = NULL;
    return n;
}

mg_region_handle mg_region_create(uint8_t max_class_bits)
{
    struct mg_region *r = heap_malloc(sizeof(struct mg_region));
    if (!r)
        return NULL;

    if (max_class_bits > MG_MAX_CLASS_BITS)
        max_class_bits = MG_MAX_CLASS_BITS;

    r->max_bits = max_class_bits;

    for (uint8_t i = 0; i <= r->max_bits; i++) {
        r->classes[i].pools = NULL;
        r->classes[i].block_size = (size_t)1u << i;
    }

    return r;
}

static void *mg_region_alloc_class(struct mg_region *r, size_t size)
{
    size_t need = size + sizeof(struct mg_block_header);
    uint8_t bits = next_power_of_two_index_u32((uint32_t)need);

    if (bits > r->max_bits)
        return NULL;

    struct mg_class *c = &r->classes[bits];

    for (struct mg_pool_node *n = c->pools; n; n = n->next) {
        void *blk = membit_alloc(n->pool);
        if (blk) {
            struct mg_block_header *h = (struct mg_block_header *)blk;
            h->pool = n->pool;
            return (uint8_t *)blk + sizeof(struct mg_block_header);
        }
    }

    struct mg_pool_node *n = mg_pool_node_new(c->block_size);
    if (!n)
        return NULL;

    n->next = c->pools;
    c->pools = n;

    void *blk = membit_alloc(n->pool);
    if (!blk)
        return NULL;

    struct mg_block_header *h = (struct mg_block_header *)blk;
    h->pool = n->pool;
    return (uint8_t *)blk + sizeof(struct mg_block_header);
}

void *mg_region_alloc(struct mg_region *r, size_t size)
{
    if (!r || !size)
        return NULL;

    if (size < MG_MIN_CLASS_BYTES)
        size = MG_MIN_CLASS_BYTES;

    return mg_region_alloc_class(r, size);
}

void mg_region_reset(struct mg_region *r)
{
    if (!r)
        return;

    for (uint8_t i = 0; i <= r->max_bits; i++) {
        for (struct mg_pool_node *n = r->classes[i].pools; n; n = n->next) {
            membit_reset(n->pool);
        }
    }
}

void mg_region_destroy(mg_region_handle r)
{
    if (!r)
        return;

    for (uint8_t i = 0; i <= r->max_bits; i++) {
        struct mg_pool_node *n = r->classes[i].pools;
        while (n) {
            struct mg_pool_node *next = n->next;
            membit_destroy(n->pool);
            heap_free(n);
            n = next;
        }
    }

    heap_free(r);
}

