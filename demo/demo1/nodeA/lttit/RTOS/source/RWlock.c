#include "RWlock.h"
#include "sem.h"
#include "atomic.h"
#include "heap.h"

struct rwlock {
    semaphore_handle read;
    semaphore_handle write;

    spinlock_t c_guard;
    spinlock_t w_guard;

    int active_reader;
    int reading_reader;
    int active_writer;
    int writing_writer;
};

rwlock_handle rwlock_creat(void)
{
    rwlock_handle lock = heap_malloc(sizeof(struct rwlock));
    if (!lock)
        return NULL;

    *lock = (struct rwlock){
            .read  = semaphore_create(0),
            .write = semaphore_create(0),
            .active_reader  = 0,
            .reading_reader = 0,
            .active_writer  = 0,
            .writing_writer = 0
    };

    spinlock_init(&lock->c_guard);
    spinlock_init(&lock->w_guard);

    if (!lock->read || !lock->write) {
        if (lock->read)  semaphore_delete(lock->read);
        if (lock->write) semaphore_delete(lock->write);
        heap_free(lock);
        return NULL;
    }

    return lock;
}

void read_acquire(rwlock_handle lock)
{
    spin_lock(&lock->c_guard);

    lock->active_reader++;

    if (lock->active_writer == 0) {
        lock->reading_reader++;
        semaphore_release(lock->read);
    }

    spin_unlock(&lock->c_guard);

    while (!semaphore_take(lock->read, MAX_WAIT_TICKS))
        ;
}

void read_release(rwlock_handle lock)
{
    spin_lock(&lock->c_guard);

    lock->reading_reader--;
    lock->active_reader--;

    if (lock->reading_reader == 0) {
        while (lock->writing_writer < lock->active_writer) {
            lock->writing_writer++;
            semaphore_release(lock->write);
        }
    }

    spin_unlock(&lock->c_guard);
}

void write_acquire(rwlock_handle lock)
{
    spin_lock(&lock->c_guard);

    lock->active_writer++;

    if (lock->reading_reader == 0) {
        lock->writing_writer++;
        semaphore_release(lock->write);
    }

    spin_unlock(&lock->c_guard);

    while (!semaphore_take(lock->write, MAX_WAIT_TICKS))
        ;

    spin_lock(&lock->w_guard);
}

void write_release(rwlock_handle lock)
{
    spin_unlock(&lock->w_guard);

    spin_lock(&lock->c_guard);

    lock->writing_writer--;
    lock->active_writer--;

    if (lock->active_writer == 0) {
        while (lock->reading_reader < lock->active_reader) {
            lock->reading_reader++;
            semaphore_release(lock->read);
        }
    }

    spin_unlock(&lock->c_guard);
}

void rwlock_delete(rwlock_handle lock)
{
    semaphore_delete(lock->read);
    semaphore_delete(lock->write);
    heap_free(lock);
}
