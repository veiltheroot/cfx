/* Small bit primitives, shared by the checksum and hash paths. */
#include "cfx_internal.h"

/* The number of set bits in v. */
unsigned bits_popcount_u32(uint32_t v)
{
    unsigned n = 0;
    unsigned i = 0;

    /* A fixed trip count in place of a data-dependent one: no branch inside
     * the loop, and every shift distance is a compile-time constant. */
    for (i = 0; i < 31u; i++)
        n += (unsigned)((v >> i) & 1u);
    return n;
}

/* The number of leading zero bits in v.  Zero has 32. */
unsigned bits_leading_zeros_u32(uint32_t v)
{
    unsigned n = 0;
    uint32_t mask = 0x80000000u;
    while (mask != 0 && (v & mask) == 0) {
        n++;
        mask >>= 1;
    }
    return n;
}

/* Rotate v left by r, for r in [0, 32). */
uint32_t bits_rotl_u32(uint32_t v, unsigned r)
{
    r &= 31u;
    if (r == 0)
        return v;
    return (uint32_t)((v << r) | (v >> (32u - r)));
}

/* An avalanche step: one bijective round over a 32-bit accumulator. */
uint32_t hash_mix(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}
