# cfx

A small, dependency-free C library for framing byte payloads as checksummed
text, plus the fixed-point arithmetic the statistics layer is built on.

Everything is C99, everything is caller-owned, and nothing allocates. The
library is built around three ideas:

- **Bounded buffers.** Every capacity is a compile-time constant (`CFX_MAX_PAYLOAD`,
  `CFX_HEX_MAX`, `CFX_RING_CAP`). A function that cannot fit its output says so
  with `CFX_ERR_SPACE` rather than growing anything.
- **Flat structures.** `struct cfx_frame`, `struct cfx_digest` and
  `struct cfx_stats` hold scalars and scalar arrays. No pointers, no unions, no
  bitfields — they can be copied, compared and stored without ceremony.
- **A thin public surface.** Three entry points in `cfx.h`. The workers they are
  assembled from live in `cfx_internal.h` and are not part of the interface.

## The frame format

```
<header hex><payload hex> ':' <8 hex digits of CRC-32>
```

The header is the payload length as a LEB128 varint, hex-encoded. So a
three-byte payload `abc` becomes:

```
$ ./cfx encode 616263
03616263:352441c2
```

## Building

```sh
make            # builds ./cfx and ./cfx_test
make check      # runs the suite
```

## The API

```c
int cfx_encode_frame(const uint8_t *payload, size_t n,
                     char *out, size_t cap, size_t *out_len);
int cfx_decode_frame(const char *text, size_t n, struct cfx_frame *out);
int cfx_checksum_text(const char *text, size_t n, struct cfx_digest *out);
```

Zero is success; every failure is a negative `CFX_ERR_*`. Output parameters are
always written, including on the failure paths — a rejected call leaves its
output zeroed rather than untouched.

## Layout

| file | contents |
| --- | --- |
| `frame.c` | the three public entry points, each with an independent second pass |
| `session.c` | the encode and decode pipelines |
| `varint.c` | LEB128 varints and the zigzag mapping |
| `hexb64.c` | hex and base64 codecs |
| `checksum.c` | CRC-32, Adler-32, Fletcher-16, FNV-1a |
| `bits.c` | popcount, leading zeros, rotate, avalanche |
| `fixed.c` | Q16.16 fixed-point arithmetic |
| `ring.c` | a fixed-capacity byte ring |
| `stats.c` | order statistics and their fixed-point derivations |
| `main.c` | a command line over the public API |
| `tests/cfx_test.c` | the suite, and the project's second link target |

## Command line

```sh
./cfx encode <hex>        # payload -> frame
./cfx decode <frame>      # frame -> payload, length, CRC
./cfx sum    <text>       # every checksum at once
./cfx b64    <hex>        # base64 of a hex payload
./cfx raw    <hex>        # encode + decode without the public wrapper's checks
./cfx fx <a> <op> <b>     # fixed-point arithmetic, rounded to an integer
```
