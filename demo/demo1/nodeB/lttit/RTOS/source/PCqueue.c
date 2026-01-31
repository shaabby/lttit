#include "PCqueue.h"
#include "sem.h"
#include "heap.h"

struct oo_buffer {
    uint8_t in;
    uint8_t out;
    uint8_t size;
    semaphore_handle item;
    semaphore_handle space;
    int buf[];
};

struct mo_buffer {
    uint8_t in;
    uint8_t out;
    uint8_t size;
    semaphore_handle item;
    semaphore_handle space;
    semaphore_handle guard;
    int buf[];
};

struct mm_buffer {
    uint8_t in;
    uint8_t out;
    uint8_t size;
    semaphore_handle item;
    semaphore_handle space;
    semaphore_handle guard;
    int buf[];
};

oo_buffer_handle Oo_buffer_creat(uint8_t buffer_size)
{
    struct oo_buffer *b;

    b = heap_malloc(sizeof(*b) + sizeof(int) * buffer_size);
    if (!b)
        return NULL;

    *b = (struct oo_buffer){
            .in = 0,
            .out = 0,
            .size = buffer_size,
            .item = semaphore_create(0),
            .space = semaphore_create(buffer_size),
    };

    return b;
}

void Oo_insert(oo_buffer_handle b, int object)
{
    while (!semaphore_take(b->space, MAX_WAIT_TICKS))
        ;

    b->buf[b->in] = object;
    b->in = (b->in + 1) % b->size;

    semaphore_release(b->item);
}

int Oo_remove(oo_buffer_handle b)
{
    int item;

    while (!semaphore_take(b->item, MAX_WAIT_TICKS))
        ;

    item = b->buf[b->out];
    b->out = (b->out + 1) % b->size;

    semaphore_release(b->space);

    return item;
}

void Oo_buffer_delete(oo_buffer_handle b)
{
    semaphore_delete(b->item);
    semaphore_delete(b->space);
    heap_free(b);
}

mo_buffer_handle Mo_buffer_creat(uint8_t buffer_size)
{
    struct mo_buffer *b;

    b = heap_malloc(sizeof(*b) + sizeof(int) * buffer_size);
    if (!b)
        return NULL;

    *b = (struct mo_buffer){
            .in = 0,
            .out = 0,
            .size = buffer_size,
            .item = semaphore_create(0),
            .space = semaphore_create(buffer_size),
            .guard = semaphore_create(1),
    };

    return b;
}

void Mo_insert(mo_buffer_handle b, int object)
{
    while (!semaphore_take(b->space, MAX_WAIT_TICKS))
        ;

    while (!semaphore_take(b->guard, MAX_WAIT_TICKS))
        ;

    b->buf[b->in] = object;
    b->in = (b->in + 1) % b->size;

    semaphore_release(b->guard);
    semaphore_release(b->item);
}

int Mo_remove(mo_buffer_handle b)
{
    int item;

    while (!semaphore_take(b->item, MAX_WAIT_TICKS))
        ;

    item = b->buf[b->out];
    b->out = (b->out + 1) % b->size;

    semaphore_release(b->space);

    return item;
}

void Mo_buffer_delete(mo_buffer_handle b)
{
    semaphore_delete(b->item);
    semaphore_delete(b->space);
    semaphore_delete(b->guard);
    heap_free(b);
}

mm_buffer_handle Mm_buffer_creat(uint8_t buffer_size)
{
    struct mm_buffer *b;

    b = heap_malloc(sizeof(*b) + sizeof(int) * buffer_size);
    if (!b)
        return NULL;

    *b = (struct mm_buffer){
            .in = 0,
            .out = 0,
            .size = buffer_size,
            .item = semaphore_create(0),
            .space = semaphore_create(buffer_size),
            .guard = semaphore_create(1),
    };

    return b;
}

void Mm_insert(mm_buffer_handle b, int object)
{
    while (!semaphore_take(b->space, MAX_WAIT_TICKS))
        ;

    while (!semaphore_take(b->guard, MAX_WAIT_TICKS))
        ;

    b->buf[b->in] = object;
    b->in = (b->in + 1) % b->size;

    semaphore_release(b->guard);
    semaphore_release(b->item);
}

int Mm_remove(mm_buffer_handle b)
{
    int item;

    while (!semaphore_take(b->item, MAX_WAIT_TICKS))
        ;

    while (!semaphore_take(b->guard, MAX_WAIT_TICKS))
        ;

    item = b->buf[b->out];
    b->out = (b->out + 1) % b->size;

    semaphore_release(b->guard);
    semaphore_release(b->space);

    return item;
}

void Mm_buffer_delete(mm_buffer_handle b)
{
    semaphore_delete(b->item);
    semaphore_delete(b->space);
    semaphore_delete(b->guard);
    heap_free(b);
}
