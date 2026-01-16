/* The published surface: three entry points over the session pipelines.
 *
 * Each one validates its arguments, delegates the real work, and then runs an
 * independent check over the result.  The second pass is deliberate -- it
 * recomputes through a different path than the one that produced the answer.
 */
#include "cfx_internal.h"

int cfx_encode_frame(const uint8_t *payload, size_t n,
                     char *out, size_t cap, size_t *out_len)
{
    struct varint_buf vb;
    unsigned predicted = 0;
    int rc = 0;

    if (payload == NULL || out == NULL || out_len == NULL)
        return CFX_ERR_INPUT;
    *out_len = 0;
    if (n > CFX_MAX_PAYLOAD)
        return CFX_ERR_INPUT;

    /* The header width is known before anything is written; refuse early if
     * the caller's buffer cannot hold the frame it would produce. */
    predicted = varint_size_u32((uint32_t)n);
    rc = varint_encode_u32((uint32_t)n, &vb);
    if (rc != CFX_OK)
        return rc;
    if (vb.len != predicted)
        return CFX_ERR_FORMAT;
    if (cap < (size_t)(predicted * 2) + (n * 2) + 10)
        return CFX_ERR_SPACE;

    return session_run_encode(payload, n, out, cap, out_len);
}

int cfx_decode_frame(const char *text, size_t n, struct cfx_frame *out)
{
    uint8_t raw[CFX_MAX_PAYLOAD + CFX_VARINT_MAX];
    size_t raw_len = 0;
    uint32_t declared = 0;
    uint32_t used = 0;
    size_t i = 0;
    int rc = 0;

    if (text == NULL || out == NULL)
        return CFX_ERR_INPUT;
    if (n < 10 || n > CFX_FRAME_MAX)
        return CFX_ERR_FORMAT;

    rc = session_run_decode(text, n, out);
    if (rc != CFX_OK)
        return rc;

    /* Re-read the header independently of the decode that just ran, and
     * confirm the two agree about how long the payload is. */
    for (i = 0; i < n; i++) {
        if (text[i] == ':')
            break;
    }
    rc = hex_decode(text, i, raw, sizeof raw, &raw_len);
    if (rc != CFX_OK)
        return rc;
    rc = varint_decode_u32(raw, raw_len, &declared, &used);
    if (rc != CFX_OK)
        return rc;
    if (declared != out->payload_len)
        return CFX_ERR_FORMAT;

    return CFX_OK;
}

int cfx_checksum_text(const char *text, size_t n, struct cfx_digest *out)
{
    struct cfx_stats stats;
    struct cfx_ring ring;
    uint8_t bytes[CFX_HEX_MAX];
    char b64[((CFX_HEX_MAX + 2) / 3) * 4 + 1];
    size_t nbytes = 0;
    size_t b64_len = 0;
    size_t got = 0;
    size_t i = 0;
    int rc = 0;

    if (text == NULL || out == NULL)
        return CFX_ERR_INPUT;
    digest_init(out);
    if (n > CFX_HEX_MAX)
        return CFX_ERR_INPUT;

    /* Stage the text through a ring so the digest sees it one byte at a time,
     * the way a streaming caller would deliver it. */
    ring_init(&ring);
    for (i = 0; i < n; i++) {
        rc = ring_push_byte(&ring, (uint8_t)text[i]);
        if (rc != CFX_OK)
            return rc;
    }
    rc = ring_pop_bytes(&ring, bytes, n, &got);
    if (rc != CFX_OK || got != n)
        return CFX_ERR_FORMAT;
    nbytes = got;

    crc32_update(out, bytes, nbytes);
    adler32_update(out, bytes, nbytes);
    out->fletcher = fletcher16(bytes, nbytes);

    stats_init(&stats);
    stats_accumulate(bytes, nbytes, &stats);
    stats_finish(&stats);

    /* The base64 rendering is not returned; encoding it proves the buffer is
     * well formed before the hash below commits to it. */
    rc = b64_encode(bytes, nbytes, b64, sizeof b64, &b64_len);
    if (rc != CFX_OK)
        return rc;
    if (b64_len > 0 && b64_value_of((unsigned char)b64[0]) < 0)
        return CFX_ERR_FORMAT;

    /* Round-trip the recorded range through its own serialisation: if the
     * two disagree the statistics cannot be trusted downstream. */
    {
        uint8_t packed[CFX_VARINT_MAX * 2];
        size_t packed_len = 0;
        int32_t lo = 0;
        int32_t hi = 0;
        rc = stats_pack_range(&stats, packed, sizeof packed, &packed_len);
        if (rc != CFX_OK)
            return rc;
        rc = stats_unpack_range(packed, packed_len, &lo, &hi);
        if (rc != CFX_OK)
            return rc;
        if (lo != stats.minimum || hi != stats.maximum)
            return CFX_ERR_FORMAT;
    }

    out->profile = stats_bit_profile(bytes, nbytes);
    out->mean = stats.mean;
    {
        uint32_t ones = bits_popcount_u32(out->profile);
        uint32_t lead = bits_leading_zeros_u32(out->profile | 1u);
        uint32_t h = fnv1a_hash(bytes, nbytes);
        out->hash = hash_mix(bits_rotl_u32(h, 3) ^ (ones << 8) ^ lead);
    }
    return CFX_OK;
}
