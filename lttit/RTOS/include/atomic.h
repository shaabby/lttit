#ifndef ATOMIC_H
#define ATOMIC_H

#include <stdint.h>

#define ATOMIC_OP_RETURN(op)                                      \
static inline uint32_t atomic_##op##_return(uint32_t i,           \
                                            uint32_t *v)          \
{                                                                 \
    uint32_t output;                                              \
    uint32_t tmp;                                                 \
    __asm volatile (                                              \
        "1: ldrex     %0, [%2]     \n"                            \
        "   " #op "    %0, %0, %3  \n"                            \
        "   strex     %1, %0, [%2] \n"                            \
        "   teq       %1, #0       \n"                            \
        "   bne       1b           \n"                            \
        : "=&r"(output), "=&r"(tmp)                               \
        : "r"(v), "r"(i)                                          \
        : "cc"                                                    \
    );                                                            \
    return output;                                                \
}

#define ATOMIC_OP(op)                                             \
static inline void atomic_##op(uint32_t i, uint32_t *v)           \
{                                                                 \
    uint32_t tmp;                                                 \
    __asm volatile (                                              \
        "1: ldrex     %0, [%2]     \n"                            \
        "   " #op "    %0, %0, %1  \n"                            \
        "   strex     %1, %0, [%2] \n"                            \
        "   teq       %1, #0       \n"                            \
        "   bne       1b           \n"                            \
        : "=&r"(tmp)                                              \
        : "r"(i), "r"(v)                                          \
        : "cc"                                                    \
    );                                                            \
}

#define ATOMIC_OPS(op)  ATOMIC_OP(op) ATOMIC_OP_RETURN(op)

ATOMIC_OPS(add)
ATOMIC_OPS(sub)

static inline uint32_t atomic_set_return(uint32_t i, uint32_t *v)
{
    uint32_t tmp, res;
    __asm volatile (
            "1: ldrex %0, [%2]     \n"
            "   strex %1, %3, [%2] \n"
            "   teq   %1, #0       \n"
            "   bne   1b           \n"
            : "=&r"(tmp), "=&r"(res)
            : "r"(v), "r"(i)
            : "cc"
            );
    return res;
}

static inline void atomic_set(uint32_t i, uint32_t *v)
{
    uint32_t tmp;
    __asm volatile (
            "1: ldrex %0, [%1]     \n"
            "   strex %0, %2, [%1] \n"
            "   teq   %0, #0       \n"
            "   bne   1b           \n"
            : "=&r"(tmp)
            : "r"(v), "r"(i)
            : "cc"
            );
}

static inline uint32_t atomic_read(const uint32_t *v)
{
    return *v;
}

#define atomic_inc(v)        atomic_add(1, (v))
#define atomic_dec(v)        atomic_sub(1, (v))
#define atomic_inc_return(v) atomic_add_return(1, (v))
#define atomic_dec_return(v) atomic_sub_return(1, (v))

static inline uint32_t atomic_cmpxchg(uint32_t *v,
                                      uint32_t old,
                                      uint32_t newv)
{
    uint32_t cur, tmp;
    __asm volatile (
            "1: ldrex %0, [%3]     \n"
            "   cmp   %0, %4       \n"
            "   bne   2f           \n"
            "   strex %1, %5, [%3] \n"
            "   teq   %1, #0       \n"
            "   bne   1b           \n"
            "2:                    \n"
            : "=&r"(cur), "=&r"(tmp)
            : "r"(v), "r"(v), "r"(old), "r"(newv)
            : "cc"
            );
    return cur;
}

extern volatile uint32_t preempt_count;

static inline void preempt_disable(void)
{
    atomic_inc((uint32_t *)&preempt_count);
}

static inline void preempt_enable(void)
{
    atomic_dec((uint32_t *)&preempt_count);
}

static inline uint32_t preempt_count_get(void)
{
    return atomic_read((const uint32_t *)&preempt_count);
}

typedef struct {
    uint32_t val;
    uint32_t owner_nesting;
} spinlock_t;

static inline void spinlock_init(spinlock_t *l)
{
    atomic_set(0, &l->val);
    l->owner_nesting = 0;
}

static inline void spin_lock(spinlock_t *l)
{
    preempt_disable();

    if (l->owner_nesting > 0) {
        l->owner_nesting++;
        return;
    }

    while (atomic_cmpxchg(&l->val, 0, 1) != 0)
        ;

    l->owner_nesting = 1;
}

static inline void spin_unlock(spinlock_t *l)
{
    if (l->owner_nesting == 0) {
        preempt_enable();
        return;
    }

    l->owner_nesting--;

    if (l->owner_nesting == 0) {
        atomic_set(0, &l->val);
    }

    preempt_enable();
}

#endif
