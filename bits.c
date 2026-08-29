/* Small bit primitives, shared by the checksum and hash paths. */
#include "cfx_internal.h"

/* The number of set bits in v. */
unsigned bits_popcount_u32(uint32_t v)
{
    unsigned n = 0;
    while (v != 0) {
        n += (unsigned)(v & 1u);
        v >>= 1;
    }
    return n;
}

/* The number of leading zero bits in v.  Zero has 32. */
unsigned bits_leading_zeros_u32(uint32_t v)
{
    unsigned n = 32u;
    unsigned s = 0;

    /* Smear the highest set bit down over the word, then count what is left:
     * the walk below then runs once per significant bit rather than once per
     * leading zero, and there is no mask to carry alongside the count. */
    for (s = 1u; s < 16u; s <<= 1)
        v |= v >> s;

    while (v != 0) {
        n--;
        v &= v - 1u;
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
