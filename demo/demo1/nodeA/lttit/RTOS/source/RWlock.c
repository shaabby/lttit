#include "RWlock.h"
#include "sem.h"
#include "heap.h"

struct rwlock {
    semaphore_handle read;
    semaphore_handle write;
    semaphore_handle w_guard;
    semaphore_handle c_guard;
    int active_reader;
    int reading_reader;
    int active_writer;
    int writing_writer;
};

rwlock_handle rwlock_creat(void)
{
    rwlock_handle lock;

    lock = heap_malloc(sizeof(struct rwlock));
    if (!lock)
        return NULL;

    *lock = (struct rwlock){
            .read = semaphore_create(0),
            .write = semaphore_create(0),
            .w_guard = semaphore_create(1),
            .c_guard = semaphore_create(1),
            .active_reader = 0,
            .reading_reader = 0,
            .active_writer = 0,
            .writing_writer = 0
    };

    return lock;
}

void read_acquire(rwlock_handle lock)
{
    while (!semaphore_take(lock->c_guard, MAX_WAIT_TICKS))
        ;

    lock->active_reader++;

    if (lock->active_writer == 0) {
        lock->reading_reader++;
        semaphore_release(lock->read);
    }

    semaphore_release(lock->c_guard);

    while (!semaphore_take(lock->read, MAX_WAIT_TICKS))
        ;
}

void read_release(rwlock_handle lock)
{
    while (!semaphore_take(lock->c_guard, MAX_WAIT_TICKS))
        ;

    lock->reading_reader--;
    lock->active_reader--;

    if (lock->reading_reader == 0) {
        while (lock->writing_writer < lock->active_writer) {
            lock->writing_writer++;
            semaphore_release(lock->write);
        }
    }

    semaphore_release(lock->c_guard);
}

void write_acquire(rwlock_handle lock)
{
    while (!semaphore_take(lock->c_guard, MAX_WAIT_TICKS))
        ;

    lock->active_writer++;

    if (lock->reading_reader == 0) {
        lock->writing_writer++;
        semaphore_release(lock->write);
    }

    semaphore_release(lock->c_guard);

    while (!semaphore_take(lock->write, MAX_WAIT_TICKS))
        ;

    while (!semaphore_take(lock->w_guard, MAX_WAIT_TICKS))
        ;
}

void write_release(rwlock_handle lock)
{
    semaphore_release(lock->w_guard);

    while (!semaphore_take(lock->c_guard, MAX_WAIT_TICKS))
        ;

    lock->writing_writer--;
    lock->active_writer--;

    if (lock->active_writer == 0) {
        while (lock->reading_reader < lock->active_reader) {
            lock->reading_reader++;
            semaphore_release(lock->read);
        }
    }

    semaphore_release(lock->c_guard);
}

void rwlock_delete(rwlock_handle lock)
{
    semaphore_delete(lock->read);
    semaphore_delete(lock->write);
    semaphore_delete(lock->w_guard);
    semaphore_delete(lock->c_guard);
    heap_free(lock);
}
