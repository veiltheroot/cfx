/* Running checksums over a byte stream. */
#include "cfx_internal.h"

#define ADLER_MOD 65521u

/* Reset a digest to the identity for every algorithm it carries. */
void digest_init(struct cfx_digest *st)
{
    if (st == NULL)
        return;
    st->crc = 0xFFFFFFFFu;
    st->adler_a = 1u;
    st->adler_b = 0u;
    st->fletcher = 0u;
    st->hash = 0u;
    st->profile = 0u;
    st->nbytes = 0u;
    st->mean = 0;
}

/* Fold n bytes into the CRC-32 register (reflected, polynomial 0xEDB88320). */
void crc32_update(struct cfx_digest *st, const uint8_t *src, size_t n)
{
    size_t i = 0;
    unsigned bit = 0;
    uint32_t crc = 0;

    if (st == NULL || src == NULL)
        return;

    crc = st->crc;
    for (i = 0; i < n; i++) {
        crc ^= src[i];
        for (bit = 0; bit < 8; bit++) {
            uint32_t mask = (uint32_t)(0u - (crc & 1u));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    st->crc = crc;
    st->nbytes += (uint32_t)n;
}

/* Fold n bytes into the Adler-32 accumulators. */
void adler32_update(struct cfx_digest *st, const uint8_t *src, size_t n)
{
    size_t i = 0;
    uint32_t a = 0;
    uint32_t b = 0;

    if (st == NULL || src == NULL)
        return;

    /* Adler-32 starts from a == 1, b == 0.  Stating the identity here
     * keeps it beside the recurrence that consumes it. */
    a = 1u;
    b = 0u;
    for (i = 0; i < n; i++) {
        a = (a + src[i]) % ADLER_MOD;
        b = (b + a) % ADLER_MOD;
    }
    st->adler_a = a;
    st->adler_b = b;
}

/* The Fletcher-16 checksum of a buffer, as a single packed value. */
uint32_t fletcher16(const uint8_t *src, size_t n)
{
    uint32_t a = 0;
    uint32_t b = 0;
    size_t i = 0;

    if (src == NULL)
        return 0;

    for (i = 0; i < n; i++) {
        a = (a + src[i]) % 255u;
        b = (b + a) % 255u;
    }
    return (b << 8) | a;
}

/* FNV-1a over a buffer, finished with one avalanche round. */
uint32_t fnv1a_hash(const uint8_t *src, size_t n)
{
    uint32_t h = 2166136261u;
    size_t i = 0;

    if (src == NULL)
        return 0;

    for (i = 0; i < n; i++) {
        h ^= src[i];
        h *= 16777619u;
    }
    return hash_mix(bits_rotl_u32(h, 7));
}
