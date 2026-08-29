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
    /* Both distances are masked, so the shift by the full word width that the
     * zero case existed to avoid cannot happen and the branch can go. */
    return (uint32_t)((v >> r) | (v << ((32u - r) & 31u)));
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
