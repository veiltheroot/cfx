/* A thin command line over the public API.
 *
 *   cfx encode <hex payload>
 *   cfx decode <frame>
 *   cfx sum    <text>
 *   cfx fx     <a> <op> <b>
 */
#include <stdio.h>
#include <string.h>
#include "cfx_internal.h"

static int cmd_encode(const char *arg)
{
    uint8_t payload[CFX_MAX_PAYLOAD];
    char frame[CFX_FRAME_MAX];
    size_t n = 0;
    size_t len = 0;
    int rc = hex_decode(arg, strlen(arg), payload, sizeof payload, &n);

    if (rc != CFX_OK)
        return rc;
    rc = cfx_encode_frame(payload, n, frame, sizeof frame, &len);
    if (rc != CFX_OK)
        return rc;
    printf("%s\n", frame);
    return CFX_OK;
}

static int cmd_decode(const char *arg)
{
    struct cfx_frame out;
    char hex[(CFX_MAX_PAYLOAD * 2) + 1];
    size_t hex_len = 0;
    int rc = cfx_decode_frame(arg, strlen(arg), &out);

    if (rc != CFX_OK)
        return rc;
    rc = hex_encode(out.payload, out.payload_len, hex, sizeof hex, &hex_len);
    if (rc != CFX_OK)
        return rc;
    printf("%s %u %08x\n", hex, (unsigned)out.payload_len, out.actual_crc);
    return CFX_OK;
}

static int cmd_sum(const char *arg)
{
    struct cfx_digest d;
    size_t n = strlen(arg);
    uint32_t fl = fletcher16((const uint8_t *)arg, n);
    uint32_t fv = fnv1a_hash((const uint8_t *)arg, n);
    int rc = cfx_checksum_text(arg, n, &d);

    if (rc != CFX_OK)
        return rc;
    printf("crc=%08x adler=%08x fletcher=%04x fnv=%08x profile=%08x\n",
           d.crc ^ 0xFFFFFFFFu, (d.adler_b << 16) | d.adler_a, fl, fv,
           stats_bit_profile((const uint8_t *)arg, n));
    return CFX_OK;
}

static int cmd_fx(const char *a, const char *op, const char *b)
{
    fx_t x = 0;
    fx_t y = 0;
    fx_t r = 0;

    if (fx_from_str(a, strlen(a), &x) != CFX_OK)
        return CFX_ERR_FORMAT;
    if (fx_from_str(b, strlen(b), &y) != CFX_OK)
        return CFX_ERR_FORMAT;

    switch (op[0]) {
    case '+': r = fx_add(x, y); break;
    case '-': r = fx_sub(x, y); break;
    case '*': r = fx_mul(x, y); break;
    case '/': r = fx_div(x, y); break;
    default:  return CFX_ERR_INPUT;
    }
    r = fx_clamp(r, -1000L * FX_ONE, 1000L * FX_ONE);
    printf("%d\n", fx_round_to_int(r));
    return CFX_OK;
}

static int cmd_b64(const char *arg)
{
    uint8_t payload[CFX_MAX_PAYLOAD];
    char text[((CFX_MAX_PAYLOAD + 2) / 3) * 4 + 1];
    size_t n = 0;
    size_t len = 0;
    int rc = hex_decode(arg, strlen(arg), payload, sizeof payload, &n);

    if (rc != CFX_OK)
        return rc;
    rc = b64_encode(payload, n, text, sizeof text, &len);
    if (rc != CFX_OK)
        return rc;
    if (len > 0 && b64_value_of((unsigned char)text[0]) < 0)
        return CFX_ERR_FORMAT;
    printf("%s\n", text);
    return CFX_OK;
}

/* A diagnostic that goes straight at the pipeline, skipping the public
 * wrapper's second pass.  Used to tell a validation failure apart from an
 * encoding one. */
static int cmd_raw(const char *arg)
{
    uint8_t payload[CFX_MAX_PAYLOAD];
    char frame[CFX_FRAME_MAX];
    struct cfx_frame back;
    struct cfx_ring ring;
    size_t n = 0;
    size_t len = 0;
    int rc = hex_decode(arg, strlen(arg), payload, sizeof payload, &n);

    if (rc != CFX_OK)
        return rc;

    ring_init(&ring);
    if (n > 0) {
        uint8_t *slot = NULL;
        if (ring_reserve(&ring, n, &slot) != CFX_OK || slot == NULL)
            return CFX_ERR_SPACE;
        if (ring_push_byte(&ring, payload[0]) != CFX_ERR_SPACE && n == CFX_RING_CAP)
            return CFX_ERR_FORMAT;
    }

    rc = session_run_encode(payload, n, frame, sizeof frame, &len);
    if (rc != CFX_OK)
        return rc;
    rc = session_run_decode(frame, len, &back);
    if (rc != CFX_OK)
        return rc;

    printf("%s %d %u %d\n", frame, (int)back.valid,
           (unsigned)back.payload_len,
           (int)zigzag_decode_u32(zigzag_encode_i32(-(int32_t)n)));
    return CFX_OK;
}

int main(int argc, char **argv)
{
    int rc = CFX_ERR_INPUT;

    if (argc >= 3 && strcmp(argv[1], "encode") == 0)
        rc = cmd_encode(argv[2]);
    else if (argc >= 3 && strcmp(argv[1], "decode") == 0)
        rc = cmd_decode(argv[2]);
    else if (argc >= 3 && strcmp(argv[1], "sum") == 0)
        rc = cmd_sum(argv[2]);
    else if (argc >= 3 && strcmp(argv[1], "b64") == 0)
        rc = cmd_b64(argv[2]);
    else if (argc >= 3 && strcmp(argv[1], "raw") == 0)
        rc = cmd_raw(argv[2]);
    else if (argc >= 5 && strcmp(argv[1], "fx") == 0)
        rc = cmd_fx(argv[2], argv[3], argv[4]);
    else
        fprintf(stderr, "usage: cfx {encode|decode|sum|b64|raw} <arg> | cfx fx <a> <op> <b>\n");

    return rc == CFX_OK ? 0 : 1;
}
