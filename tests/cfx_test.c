/* The API-level suite.
 *
 * Every case goes through cfx.h.  The suite is the project's link target, so
 * these call sites are the ones a whole-program analysis sees: the arguments
 * built here are how a caller really builds them.
 */
#include <stdio.h>
#include <string.h>
#include "../cfx.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do {                                                     \
    checks++;                                                                \
    if (!(cond)) {                                                           \
        failures++;                                                          \
        printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);               \
    }                                                                        \
} while (0)

/* A round trip through both directions, for one payload. */
static void round_trip(const uint8_t *payload, size_t n, const char *label)
{
    char frame[CFX_FRAME_MAX];
    struct cfx_frame back;
    size_t len = 0;
    size_t i = 0;
    int rc = 0;

    memset(frame, 0, sizeof frame);
    memset(&back, 0, sizeof back);

    rc = cfx_encode_frame(payload, n, frame, sizeof frame, &len);
    CHECK(rc == CFX_OK);
    if (rc != CFX_OK) {
        printf("  (encode failed for %s)\n", label);
        return;
    }
    CHECK(len == strlen(frame));
    CHECK(frame[len - 9] == ':');

    rc = cfx_decode_frame(frame, len, &back);
    CHECK(rc == CFX_OK);
    if (rc != CFX_OK) {
        printf("  (decode failed for %s)\n", label);
        return;
    }
    CHECK(back.valid == 1);
    CHECK(back.payload_len == (uint32_t)n);
    CHECK(back.actual_crc == back.declared_crc);
    for (i = 0; i < n; i++)
        CHECK(back.payload[i] == payload[i]);
}

static void test_round_trips(void)
{
    uint8_t buf[CFX_MAX_PAYLOAD];
    size_t i = 0;

    round_trip((const uint8_t *)"", 0, "empty");
    round_trip((const uint8_t *)"a", 1, "one byte");
    round_trip((const uint8_t *)"hello", 5, "short ascii");
    round_trip((const uint8_t *)"\x00\x01\x02\x03", 4, "low bytes");
    round_trip((const uint8_t *)"\xff\xfe\xfd", 3, "high bytes");

    /* A boundary payload: exactly the maximum the library accepts. */
    for (i = 0; i < CFX_MAX_PAYLOAD; i++)
        buf[i] = (uint8_t)(i * 7u);
    round_trip(buf, CFX_MAX_PAYLOAD, "maximal");

    /* One byte under, and the varint header width boundary at 128. */
    round_trip(buf, CFX_MAX_PAYLOAD - 1, "maximal less one");
    round_trip(buf, 63, "just under a two-byte header");
}

static void test_encode_rejects(void)
{
    uint8_t buf[CFX_MAX_PAYLOAD + 8];
    char out[CFX_FRAME_MAX];
    char tiny[4];
    size_t len = 0;

    memset(buf, 0x5A, sizeof buf);

    CHECK(cfx_encode_frame(NULL, 4, out, sizeof out, &len) == CFX_ERR_INPUT);
    CHECK(cfx_encode_frame(buf, 4, NULL, sizeof out, &len) == CFX_ERR_INPUT);
    CHECK(cfx_encode_frame(buf, 4, out, sizeof out, NULL) == CFX_ERR_INPUT);
    CHECK(cfx_encode_frame(buf, CFX_MAX_PAYLOAD + 1, out, sizeof out, &len) == CFX_ERR_INPUT);
    CHECK(cfx_encode_frame(buf, 8, tiny, sizeof tiny, &len) == CFX_ERR_SPACE);
}

static void test_decode_rejects(void)
{
    struct cfx_frame out;
    char frame[CFX_FRAME_MAX];
    size_t len = 0;

    memset(&out, 0, sizeof out);

    CHECK(cfx_decode_frame(NULL, 12, &out) == CFX_ERR_INPUT);
    CHECK(cfx_decode_frame("0161:00000000", 13, NULL) == CFX_ERR_INPUT);
    CHECK(cfx_decode_frame("short", 5, &out) == CFX_ERR_FORMAT);
    CHECK(cfx_decode_frame("0161zz:0000000", 14, &out) == CFX_ERR_FORMAT);
    CHECK(cfx_decode_frame("0161000000000", 13, &out) == CFX_ERR_FORMAT);

    /* A frame whose CRC does not match its payload. */
    CHECK(cfx_encode_frame((const uint8_t *)"abc", 3, frame, sizeof frame, &len) == CFX_OK);
    frame[len - 1] = (frame[len - 1] == '0') ? '1' : '0';
    CHECK(cfx_decode_frame(frame, len, &out) == CFX_ERR_CRC);
    CHECK(out.valid == 0);

    /* A frame whose declared length disagrees with the payload it carries. */
    CHECK(cfx_encode_frame((const uint8_t *)"abcd", 4, frame, sizeof frame, &len) == CFX_OK);
    frame[0] = '0';
    frame[1] = '9';
    CHECK(cfx_decode_frame(frame, len, &out) != CFX_OK);
}

static void test_checksums(void)
{
    struct cfx_digest a;
    struct cfx_digest b;
    struct cfx_digest c;

    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    memset(&c, 0, sizeof c);

    CHECK(cfx_checksum_text("", 0, &a) == CFX_OK);
    CHECK(a.nbytes == 0);
    CHECK(a.adler_a == 1);
    CHECK(a.adler_b == 0);

    CHECK(cfx_checksum_text("abc", 3, &b) == CFX_OK);
    CHECK(b.nbytes == 3);
    CHECK((b.crc ^ 0xFFFFFFFFu) == 0x352441C2u);   /* CRC-32 of "abc" */
    CHECK(b.adler_a == 1 + 'a' + 'b' + 'c');

    /* The same bytes must digest identically, twice. */
    CHECK(cfx_checksum_text("abc", 3, &c) == CFX_OK);
    CHECK(c.crc == b.crc);
    CHECK(c.hash == b.hash);
    CHECK(c.fletcher == b.fletcher);
    CHECK(c.profile == b.profile);

    /* Different bytes must not. */
    CHECK(cfx_checksum_text("abd", 3, &c) == CFX_OK);
    CHECK(c.crc != b.crc);

    /* "abc" is 97, 98, 99: mean exactly 98, spread exactly 2. */
    CHECK(b.mean == (fx_t)(98L << FX_SHIFT));

    CHECK(cfx_checksum_text(NULL, 3, &a) == CFX_ERR_INPUT);
    CHECK(cfx_checksum_text("x", 1, NULL) == CFX_ERR_INPUT);
}

/* The derived fixed-point figures must agree with the integers they come
 * from, for every single-byte input. */
static void test_derived_stats(void)
{
    struct cfx_digest d;
    char one[1];
    int v = 0;

    for (v = 0; v < 256; v++) {
        one[0] = (char)v;
        memset(&d, 0, sizeof d);
        CHECK(cfx_checksum_text(one, 1, &d) == CFX_OK);
        /* A single byte is its own mean, exactly and without rounding. */
        CHECK(d.mean == (fx_t)((int32_t)(unsigned char)one[0] << FX_SHIFT));
    }
}

static void test_checksum_boundaries(void)
{
    struct cfx_digest d;
    char big[CFX_HEX_MAX + 4];
    size_t i = 0;

    for (i = 0; i < sizeof big; i++)
        big[i] = (char)('A' + (i % 26));

    memset(&d, 0, sizeof d);
    CHECK(cfx_checksum_text(big, CFX_HEX_MAX, &d) == CFX_OK);
    CHECK(d.nbytes == CFX_HEX_MAX);
    CHECK(cfx_checksum_text(big, CFX_HEX_MAX + 1, &d) == CFX_ERR_INPUT);
}

/* Every frame the encoder produces must survive a decode, for every length
 * from empty to maximal.  This is the broadest sweep the suite runs. */
static void test_all_lengths(void)
{
    uint8_t buf[CFX_MAX_PAYLOAD];
    char frame[CFX_FRAME_MAX];
    struct cfx_frame back;
    size_t n = 0;
    size_t i = 0;
    size_t len = 0;

    for (i = 0; i < CFX_MAX_PAYLOAD; i++)
        buf[i] = (uint8_t)((i * 31u) ^ 0xA5u);

    for (n = 0; n <= CFX_MAX_PAYLOAD; n++) {
        memset(&back, 0, sizeof back);
        if (cfx_encode_frame(buf, n, frame, sizeof frame, &len) != CFX_OK) {
            CHECK(0);
            continue;
        }
        if (cfx_decode_frame(frame, len, &back) != CFX_OK) {
            CHECK(0);
            continue;
        }
        CHECK(back.payload_len == (uint32_t)n);
        CHECK(memcmp(back.payload, buf, n) == 0);
    }
}

int main(void)
{
    test_round_trips();
    test_encode_rejects();
    test_decode_rejects();
    test_checksums();
    test_derived_stats();
    test_checksum_boundaries();
    test_all_lengths();

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
