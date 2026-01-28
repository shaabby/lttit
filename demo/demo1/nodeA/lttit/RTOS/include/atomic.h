
#ifndef ATOMIC_H
#define ATOMIC_H
#include<stdint.h>

#define ATOMIC_OP_RETURN(op)                                \
static inline int atomic_##op##_return( uint32_t i,uint32_t *v)        \
{                                                           \
    uint32_t output;                                             \
    uint32_t tmp;                                                \
    __asm volatile (                                        \
    "1: ldrex     %0,[%2]		\n"                         \
    "   "#op"   %0,%0,%3   		\n"                         \
    "	strex     %1,%0,[%2]		\n"                     \
    "   teq     %1,#0           \n"                         \
    "   bne     1b              \n"                         \
    :   "=&r" (output),"=&r"(tmp)                           \
    :   "r" (v),"r"(i)                                      \
    :   "cc"                                                \
    );                                                      \
    return  output;                                         \
}


#define ATOMIC_OP(op)                                \
static inline void atomic_##op( uint32_t i,uint32_t *v)        \
{                                                           \
    uint32_t tmp;                                                \
    __asm volatile (                                        \
    "1: ldrex     %0,[%2]		\n"                         \
    "   "#op"   %0,%0,%1   		\n"                         \
    "	strex     %1,%0,[%2]		\n"                         \
    "   teq     %1,#0           \n"                         \
    "   bne     1b              \n"                         \
    :   "=&r"(tmp)                           \
    :   "r" (i),"r"(v)                                      \
    :   "cc"                                                \
    );                                                      \
}

#define ATOMIC_OPS(op)  ATOMIC_OP(op) ATOMIC_OP_RETURN(op)

ATOMIC_OPS(add)

ATOMIC_OPS(sub)

#define atomic_inc(v) (atomic_add(1,v))
#define atomic_dec(v) (atomic_sub(1,v))


#define atomic_inc_return(v) (atomic_add_return(1,v))
#define atomic_dec_return(v) (atomic_sub_return(1,v))



static inline uint32_t atomic_set_return(uint32_t i, const uint32_t *v) {
    uint32_t tmp, res;
    __asm volatile (
            "1: ldrex %0, [%2]     \n"
            "   strex %1, %3, [%2] \n"
            "   teq %1, #0         \n"
            "   bne 1b             \n"
            : "=&r" (tmp), "=&r" (res)
            : "r" (v), "r" (i)
            : "cc"
            );
    return res;
}



static inline void atomic_set(uint32_t i, const uint32_t *v) {
    uint32_t tmp;
    __asm volatile (
            "1: ldrex %0, [%1]     \n"
            "   strex %0, %2, [%1] \n"
            "   teq %0, #0         \n"
            "   bne 1b             \n"
            : "=&r" (tmp)
            : "r" (v), "r" (i)
            : "cc"
            );
}







#endif
