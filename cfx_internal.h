/* cfx internals -- the worker functions the public API is assembled from.
 *
 * Nothing here is part of the published interface.  Every function is called
 * from at least two other translation units.
 */
#ifndef CFX_INTERNAL_H
#define CFX_INTERNAL_H

#include "cfx.h"

/* A varint encoding buffer.  Caller-owned, flat scalars only. */
struct varint_buf {
    uint8_t  bytes[CFX_VARINT_MAX];
    uint32_t len;
    uint32_t written;
};

/* A byte ring.  Caller-owned; the storage is inline, never heap. */
struct cfx_ring {
    uint8_t  data[CFX_RING_CAP];
    uint32_t head;
    uint32_t count;
};

/* ---- varint.c ---- */
unsigned varint_size_u32(uint32_t v);
int      varint_encode_u32(uint32_t v, struct varint_buf *out);
int      varint_decode_u32(const uint8_t *src, size_t n,
                           uint32_t *value_out, uint32_t *used_out);
uint32_t zigzag_encode_i32(int32_t v);
int32_t  zigzag_decode_u32(uint32_t v);

/* ---- hexb64.c ---- */
int hex_digit_value(int c);
int hex_encode(const uint8_t *src, size_t n, char *out, size_t cap, size_t *out_len);
int hex_decode(const char *src, size_t n, uint8_t *out, size_t cap, size_t *out_len);
int b64_value_of(int c);
int b64_encode(const uint8_t *src, size_t n, char *out, size_t cap, size_t *out_len);

/* ---- checksum.c ---- */
void     digest_init(struct cfx_digest *st);
void     crc32_update(struct cfx_digest *st, const uint8_t *src, size_t n);
void     adler32_update(struct cfx_digest *st, const uint8_t *src, size_t n);
uint32_t fletcher16(const uint8_t *src, size_t n);
uint32_t fnv1a_hash(const uint8_t *src, size_t n);

/* ---- bits.c ---- */
unsigned bits_popcount_u32(uint32_t v);
unsigned bits_leading_zeros_u32(uint32_t v);
uint32_t bits_rotl_u32(uint32_t v, unsigned r);
uint32_t hash_mix(uint32_t h);

/* ---- fixed.c ---- */
fx_t    fx_add(fx_t a, fx_t b);
fx_t    fx_sub(fx_t a, fx_t b);
fx_t    fx_mul(fx_t a, fx_t b);
fx_t    fx_div(fx_t a, fx_t b);
int32_t fx_round_to_int(fx_t x);
fx_t    fx_clamp(fx_t x, fx_t lo, fx_t hi);
int     fx_from_str(const char *s, size_t n, fx_t *out);

/* ---- ring.c ---- */
void ring_init(struct cfx_ring *r);
int  ring_push_byte(struct cfx_ring *r, uint8_t b);
int  ring_pop_bytes(struct cfx_ring *r, uint8_t *out, size_t want, size_t *got);
int  ring_reserve(struct cfx_ring *r, size_t n, uint8_t **slot_out);

/* ---- stats.c ---- */
void     stats_init(struct cfx_stats *st);
void     stats_accumulate(const uint8_t *src, size_t n, struct cfx_stats *st);
void     stats_min_max_update(struct cfx_stats *st, int32_t v);
uint32_t stats_bit_profile(const uint8_t *src, size_t n);
void     stats_finish(struct cfx_stats *st);
int      stats_pack_range(const struct cfx_stats *st, uint8_t *out, size_t cap,
                          size_t *out_len);
int      stats_unpack_range(const uint8_t *src, size_t n,
                            int32_t *min_out, int32_t *max_out);

/* ---- session.c ---- */
int session_run_encode(const uint8_t *payload, size_t n,
                       char *out, size_t cap, size_t *out_len);
int session_run_decode(const char *text, size_t n, struct cfx_frame *out);

#endif /* CFX_INTERNAL_H */
