
#ifndef _MATH_H
#define _MATH_H
#include <stdint.h>

__attribute__((always_inline)) inline uint8_t log2_clz64(uint64_t value)
{
    return 63 - __builtin_clzll(value);
}


#endif
