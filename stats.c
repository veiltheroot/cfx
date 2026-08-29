/* Order statistics over a byte stream. */
#include "cfx_internal.h"

/* Reset the accumulator.  The extremes start inverted so the first byte wins
 * both of them. */
void stats_init(struct cfx_stats *st)
{
    if (st == NULL)
        return;
    st->nbytes = 0;
    st->minimum = INT32_MAX;
    st->maximum = INT32_MIN;
    st->sum = 0;
    st->mean = 0;
    st->spread = 0;
    st->midpoint = 0;
    st->normalized = 0;
    st->mean_rounded = 0;
}

/* Widen the recorded range to include v. */
void stats_min_max_update(struct cfx_stats *st, int32_t v)
{
    if (st == NULL)
        return;
    if (v < st->minimum)
        st->minimum = v;
    if (v > st->maximum)
        st->maximum = v;
}

/* Fold n bytes into the accumulator. */
void stats_accumulate(const uint8_t *src, size_t n, struct cfx_stats *st)
{
    size_t i = 0;

    if (src == NULL || st == NULL)
        return;

    for (i = 0; i < n; i++) {
        int32_t v = (int32_t)src[i];
        stats_min_max_update(st, v);
        st->sum += v;
        st->nbytes++;
    }
}

/* A coarse profile of a buffer: the total set-bit count in the low half, and
 * the leading-zero count of the packed high half.  Used as a cheap shape hint
 * beside the exact statistics above. */
uint32_t stats_bit_profile(const uint8_t *src, size_t n)
{
    uint32_t packed = 0;
    uint32_t ones = 0;
    size_t i = 0;

    if (src == NULL)
        return 0;

    for (i = 0; i < n; i++) {
        ones += bits_popcount_u32(src[i]);
        packed = (packed << 1) | (uint32_t)(src[i] & 1u);
    }
    return (ones << 8) | bits_leading_zeros_u32(packed);
}

/* Close the accumulator: derive the mean as a fixed-point value.
 *
 * An empty buffer has no mean; it is left at zero rather than invented. */
void stats_finish(struct cfx_stats *st)
{
    fx_t total = 0;
    fx_t count = 0;

    if (st == NULL)
        return;
    if (st->nbytes == 0) {
        st->mean = 0;
        return;
    }

    total = (fx_t)(st->sum << FX_SHIFT);
    count = (fx_t)((int32_t)st->nbytes << FX_SHIFT);
    st->mean = fx_clamp(fx_div(total, count), 0, (fx_t)(255L << FX_SHIFT));

    {
        fx_t lo = (fx_t)(st->minimum << FX_SHIFT);
        fx_t hi = (fx_t)(st->maximum << FX_SHIFT);
        fx_t two = (fx_t)(2L << FX_SHIFT);
        /* Full scale for a byte, so the normalized mean lands in [0, 1]. */
        fx_t full = (fx_t)(255L << FX_SHIFT);

        st->spread = fx_sub(hi, lo);
        st->midpoint = fx_div(fx_add(lo, hi), two);
        /* Divide by full scale rather than multiplying by a reciprocal that
         * Q16.16 cannot hold exactly: FX_ONE / 255 truncates. */
        st->normalized = fx_div(st->mean, full);
        st->mean_rounded = fx_round_to_int(st->mean);
    }
}

/* Serialise the recorded range as two zigzag varints.
 *
 * Zigzag first because the extremes are signed and usually small in
 * magnitude, which is exactly the case varints are cheap for. */
int stats_pack_range(const struct cfx_stats *st, uint8_t *out, size_t cap,
                     size_t *out_len)
{
    struct varint_buf lo;
    struct varint_buf hi;
    size_t o = 0;
    uint32_t i = 0;
    int rc = 0;

    if (st == NULL || out == NULL || out_len == NULL)
        return CFX_ERR_INPUT;
    *out_len = 0;

    rc = varint_encode_u32(zigzag_encode_i32(st->minimum), &lo);
    if (rc != CFX_OK)
        return rc;
    rc = varint_encode_u32(zigzag_encode_i32(st->maximum), &hi);
    if (rc != CFX_OK)
        return rc;
    if (cap < (size_t)lo.len + hi.len)
        return CFX_ERR_SPACE;

    for (i = 0; i < lo.len; i++)
        out[o++] = lo.bytes[i];
    for (i = 0; i < hi.len; i++)
        out[o++] = hi.bytes[i];

    *out_len = o;
    return CFX_OK;
}

/* Read back what stats_pack_range wrote. */
int stats_unpack_range(const uint8_t *src, size_t n, int32_t *min_out, int32_t *max_out)
{
    uint32_t lo = 0;
    uint32_t hi = 0;
    uint32_t used_lo = 0;
    uint32_t used_hi = 0;
    int rc = 0;

    if (src == NULL || min_out == NULL || max_out == NULL)
        return CFX_ERR_INPUT;
    *min_out = 0;
    *max_out = 0;

    rc = varint_decode_u32(src, n, &lo, &used_lo);
    if (rc != CFX_OK)
        return rc;
    rc = varint_decode_u32(src + used_lo, n - used_lo, &hi, &used_hi);
    if (rc != CFX_OK)
        return rc;

    *min_out = zigzag_decode_u32(lo);
    *max_out = zigzag_decode_u32(hi);
    return CFX_OK;
}
