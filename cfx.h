/* cfx -- a small codec and fixed-point library.
 *
 * The public API is three entry points.  Everything they are built from lives
 * in cfx_internal.h and is not part of this interface.
 */
#ifndef CFX_H
#define CFX_H

#include <stddef.h>
#include <stdint.h>

/* Capacities.  Every buffer in this library is caller-owned and bounded. */
#define CFX_MAX_PAYLOAD 64
#define CFX_HEX_MAX     64
#define CFX_RING_CAP    64
#define CFX_VARINT_MAX  5
#define CFX_FRAME_MAX   192

/* Return codes.  Zero is success; every failure is negative. */
#define CFX_OK          0
#define CFX_ERR_INPUT  (-1)
#define CFX_ERR_SPACE  (-2)
#define CFX_ERR_FORMAT (-3)
#define CFX_ERR_CRC    (-4)

/* Q16.16 fixed point. */
typedef int32_t fx_t;
#define FX_SHIFT 16
#define FX_ONE   (1L << FX_SHIFT)

/* A decoded frame.  Flat scalars and one scalar array: no pointers, no unions,
 * no bitfields, and the caller owns the storage. */
struct cfx_frame {
    uint8_t  payload[CFX_MAX_PAYLOAD];
    uint32_t payload_len;
    uint32_t declared_crc;
    uint32_t actual_crc;
    int32_t  valid;
};

/* A running digest over a byte stream. */
struct cfx_digest {
    uint32_t crc;
    uint32_t adler_a;
    uint32_t adler_b;
    uint32_t fletcher;
    uint32_t hash;
    uint32_t profile;
    uint32_t nbytes;
    fx_t     mean;
};

/* Order statistics over a byte stream. */
struct cfx_stats {
    uint32_t nbytes;
    int32_t  minimum;
    int32_t  maximum;
    int64_t  sum;
    fx_t     mean;
    fx_t     spread;
    fx_t     midpoint;
    fx_t     normalized;
    int32_t  mean_rounded;
};

/* Encode *payload* as a hex frame with a trailing CRC into *out*. */
int cfx_encode_frame(const uint8_t *payload, size_t n,
                     char *out, size_t cap, size_t *out_len);

/* Decode a hex frame, verifying its CRC. */
int cfx_decode_frame(const char *text, size_t n, struct cfx_frame *out);

/* Digest a text buffer without decoding it. */
int cfx_checksum_text(const char *text, size_t n, struct cfx_digest *out);

#endif /* CFX_H */
