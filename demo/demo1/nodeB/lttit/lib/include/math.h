#ifndef MATH_H
#define MATH_H

#include <stdint.h>

static inline uint8_t log2_clz64(uint64_t value)
{
    return 63 - __builtin_clzll(value);
}

static inline uint64_t next_power_of_two_u64(uint64_t x)
{
    if (x <= 1)
        return 1;

    uint8_t msb = log2_clz64(x - 1);
    return 1ULL << (msb + 1);
}

static inline uint8_t math_log_partition_height_u64(uint64_t x, uint8_t part_bits)
{
    if (x == 0)
        return 1;

    uint8_t msb = log2_clz64(x);
    return msb / part_bits + 1;
}

#endif
