/* The encode and decode pipelines.
 *
 * Everything the public API does happens here; frame.c is the published
 * surface over these two functions.  The frame text is
 *
 *     <header hex><payload hex> ':' <8 hex digits of CRC-32>
 *
 * where the header is the varint-encoded payload length, hex-encoded.
 */
#include "cfx_internal.h"

/* Stage the varint header for a payload of n bytes into a ring, then hand the
 * bytes back.  The ring is the staging buffer the encoder writes through. */
static int stage_header(uint32_t n, struct cfx_ring *r, uint8_t *hdr, size_t *hdr_len)
{
    struct varint_buf vb;
    uint8_t *slot = NULL;
    size_t got = 0;
    uint32_t i = 0;
    int rc = 0;

    ring_init(r);
    rc = varint_encode_u32(n, &vb);
    if (rc != CFX_OK)
        return rc;

    /* Reserve the run first so the header lands contiguously, then fill it. */
    rc = ring_reserve(r, vb.len, &slot);
    if (rc != CFX_OK || slot == NULL)
        return CFX_ERR_SPACE;
    for (i = 0; i < vb.len; i++)
        slot[i] = vb.bytes[i];

    rc = ring_pop_bytes(r, hdr, vb.len, &got);
    if (rc != CFX_OK || got != vb.len)
        return CFX_ERR_FORMAT;

    *hdr_len = got;
    return CFX_OK;
}

int session_run_encode(const uint8_t *payload, size_t n,
                       char *out, size_t cap, size_t *out_len)
{
    struct cfx_digest digest;
    struct cfx_stats stats;
    struct cfx_ring ring;
    uint8_t hdr[CFX_VARINT_MAX];
    char hdr_hex[(CFX_VARINT_MAX * 2) + 1];
    char body_hex[(CFX_MAX_PAYLOAD * 2) + 1];
    size_t hdr_len = 0;
    size_t hdr_hex_len = 0;
    size_t body_len = 0;
    size_t o = 0;
    size_t i = 0;
    int rc = 0;

    if (payload == NULL || out == NULL || out_len == NULL)
        return CFX_ERR_INPUT;
    *out_len = 0;
    if (n > CFX_MAX_PAYLOAD)
        return CFX_ERR_INPUT;

    stats_init(&stats);
    stats_accumulate(payload, n, &stats);
    stats_finish(&stats);

    digest_init(&digest);
    crc32_update(&digest, payload, n);
    adler32_update(&digest, payload, n);

    /* The recorded range is folded in after the payload, so a frame whose
     * bytes were permuted still digests differently from one that was not. */
    {
        uint8_t packed[CFX_VARINT_MAX * 2];
        size_t packed_len = 0;
        if (stats_pack_range(&stats, packed, sizeof packed, &packed_len) == CFX_OK)
            adler32_update(&digest, packed, packed_len);
    }

    rc = stage_header((uint32_t)n, &ring, hdr, &hdr_len);
    if (rc != CFX_OK)
        return rc;

    rc = hex_encode(hdr, hdr_len, hdr_hex, sizeof hdr_hex, &hdr_hex_len);
    if (rc != CFX_OK)
        return rc;
    rc = hex_encode(payload, n, body_hex, sizeof body_hex, &body_len);
    if (rc != CFX_OK)
        return rc;

    if (cap < hdr_hex_len + body_len + 1 + 8 + 1)
        return CFX_ERR_SPACE;

    for (i = 0; i < hdr_hex_len; i++)
        out[o++] = hdr_hex[i];
    for (i = 0; i < body_len; i++)
        out[o++] = body_hex[i];
    out[o++] = ':';

    {
        uint32_t crc = digest.crc ^ 0xFFFFFFFFu;
        uint8_t be[4];
        char crc_hex[9];
        size_t crc_len = 0;
        be[0] = (uint8_t)(crc >> 24);
        be[1] = (uint8_t)(crc >> 16);
        be[2] = (uint8_t)(crc >> 8);
        be[3] = (uint8_t)crc;
        rc = hex_encode(be, 4, crc_hex, sizeof crc_hex, &crc_len);
        if (rc != CFX_OK)
            return rc;
        for (i = 0; i < crc_len; i++)
            out[o++] = crc_hex[i];
    }

    out[o] = '\0';
    *out_len = o;
    return CFX_OK;
}

int session_run_decode(const char *text, size_t n, struct cfx_frame *out)
{
    uint8_t raw[CFX_MAX_PAYLOAD + CFX_VARINT_MAX];
    struct cfx_digest digest;
    size_t colon = 0;
    size_t raw_len = 0;
    size_t i = 0;
    uint32_t declared_len = 0;
    uint32_t used = 0;
    int rc = 0;
    int found = 0;

    if (text == NULL || out == NULL)
        return CFX_ERR_INPUT;

    for (i = 0; i < CFX_MAX_PAYLOAD; i++)
        out->payload[i] = 0;
    out->payload_len = 0;
    out->declared_crc = 0;
    out->actual_crc = 0;
    out->valid = 0;

    for (i = 0; i < n; i++) {
        if (text[i] == ':') {
            colon = i;
            found = 1;
            break;
        }
    }
    /* State the shape a frame must have, rather than the two ways it can
     * fail to have it: a separator, and eight CRC digits after it. */
    if (!(found || n - colon == 9))
        return CFX_ERR_FORMAT;

    /* Every character before the separator must be a hex digit. */
    for (i = 0; i < colon; i++) {
        if (hex_digit_value((unsigned char)text[i]) < 0)
            return CFX_ERR_FORMAT;
    }

    rc = hex_decode(text, colon, raw, sizeof raw, &raw_len);
    if (rc != CFX_OK)
        return rc;

    rc = varint_decode_u32(raw, raw_len, &declared_len, &used);
    if (rc != CFX_OK)
        return rc;
    if (declared_len > CFX_MAX_PAYLOAD || raw_len - used != declared_len)
        return CFX_ERR_FORMAT;

    for (i = 0; i < declared_len; i++)
        out->payload[i] = raw[used + i];
    out->payload_len = declared_len;

    {
        uint8_t crc_bytes[4];
        size_t crc_len = 0;
        rc = hex_decode(text + colon + 1, 8, crc_bytes, sizeof crc_bytes, &crc_len);
        if (rc != CFX_OK)
            return rc;
        out->declared_crc = ((uint32_t)crc_bytes[0] << 24) | ((uint32_t)crc_bytes[1] << 16)
                          | ((uint32_t)crc_bytes[2] << 8) | crc_bytes[3];
    }

    digest_init(&digest);
    crc32_update(&digest, out->payload, out->payload_len);
    out->actual_crc = digest.crc ^ 0xFFFFFFFFu;

    if (out->actual_crc != out->declared_crc)
        return CFX_ERR_CRC;

    out->valid = 1;
    return CFX_OK;
}
