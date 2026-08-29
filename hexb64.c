/* Hex and base64 text codecs. */
#include "cfx_internal.h"

static const char HEX_DIGITS[] = "0123456789abcdef";
static const char B64_ALPHABET[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* The value of one hex digit, or -1 if c is not one.  Case-insensitive. */
int hex_digit_value(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/* Write 2*n lowercase hex characters plus a NUL into out. */
int hex_encode(const uint8_t *src, size_t n, char *out, size_t cap, size_t *out_len)
{
    size_t i = 0;

    if (src == NULL || out == NULL || out_len == NULL)
        return CFX_ERR_INPUT;
    *out_len = 0;
    if (n > CFX_HEX_MAX)
        return CFX_ERR_INPUT;
    if (cap < (n * 2) + 1)
        return CFX_ERR_SPACE;

    for (i = 0; i < n; i++) {
        out[i * 2]       = HEX_DIGITS[(src[i] >> 4) & 0x0Fu];
        out[(i * 2) + 1] = HEX_DIGITS[src[i] & 0x0Fu];
    }
    out[n * 2] = '\0';
    *out_len = n * 2;
    return CFX_OK;
}

/* Read 2*k hex characters into k bytes.  An odd length is a format error. */
int hex_decode(const char *src, size_t n, uint8_t *out, size_t cap, size_t *out_len)
{
    size_t i = 0;

    if (src == NULL || out == NULL || out_len == NULL)
        return CFX_ERR_INPUT;
    *out_len = 0;
    if ((n % 2) != 0)
        return CFX_ERR_FORMAT;
    if (cap < n / 2)
        return CFX_ERR_SPACE;

    for (i = 0; i < n; i += 2) {
        int hi = hex_digit_value((unsigned char)src[i]);
        int lo = hex_digit_value((unsigned char)src[i + 1]);
        if (hi < 0 || lo < 0)
            return CFX_ERR_FORMAT;
        out[i / 2] = (uint8_t)((hi << 4) | lo);
    }
    *out_len = n / 2;
    return CFX_OK;
}

/* The value of one base64 character, or -1.  '=' is padding, not a value. */
int b64_value_of(int c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 26;
    if (c >= '0' && c <= '9')
        return c - '0' + 52;
    if (c == '+')
        return 62;
    if (c == '/')
        return 63;
    return -1;
}

/* Standard base64 with '=' padding, plus a NUL. */
int b64_encode(const uint8_t *src, size_t n, char *out, size_t cap, size_t *out_len)
{
    size_t i = 0;
    size_t o = 0;
    size_t groups = 0;
    size_t tail = 0;

    if (src == NULL || out == NULL || out_len == NULL)
        return CFX_ERR_INPUT;
    *out_len = 0;
    if (n > CFX_HEX_MAX)
        return CFX_ERR_INPUT;

    groups = n / 3;
    tail = n % 3;
    /* The capacity a caller has to supply is the number of CHARACTERS
     * written; the terminator is not one of them. */
    if (cap < (groups + (tail ? 1 : 0)) * 4)
        return CFX_ERR_SPACE;

    for (i = 0; i + 3 <= n; i += 3) {
        uint32_t w = ((uint32_t)src[i] << 16) | ((uint32_t)src[i + 1] << 8) | src[i + 2];
        out[o++] = B64_ALPHABET[(w >> 18) & 0x3Fu];
        out[o++] = B64_ALPHABET[(w >> 12) & 0x3Fu];
        out[o++] = B64_ALPHABET[(w >> 6) & 0x3Fu];
        out[o++] = B64_ALPHABET[w & 0x3Fu];
    }
    if (tail == 1) {
        uint32_t w = (uint32_t)src[i] << 16;
        out[o++] = B64_ALPHABET[(w >> 18) & 0x3Fu];
        out[o++] = B64_ALPHABET[(w >> 12) & 0x3Fu];
        out[o++] = '=';
        out[o++] = '=';
    } else if (tail == 2) {
        uint32_t w = ((uint32_t)src[i] << 16) | ((uint32_t)src[i + 1] << 8);
        out[o++] = B64_ALPHABET[(w >> 18) & 0x3Fu];
        out[o++] = B64_ALPHABET[(w >> 12) & 0x3Fu];
        out[o++] = B64_ALPHABET[(w >> 6) & 0x3Fu];
        out[o++] = '=';
    }
    out[o] = '\0';
    *out_len = o;
    return CFX_OK;
}
