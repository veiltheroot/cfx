/* Q16.16 fixed-point arithmetic. */
#include "cfx_internal.h"

/* a + b, saturating at the representable range. */
fx_t fx_add(fx_t a, fx_t b)
{
    int64_t r = (int64_t)a + (int64_t)b;
    if (r > INT32_MAX)
        return INT32_MAX;
    if (r < INT32_MIN)
        return INT32_MIN;
    return (fx_t)r;
}

/* a - b, saturating.  Not commutative: a is the minuend. */
fx_t fx_sub(fx_t a, fx_t b)
{
    int64_t r = (int64_t)a - (int64_t)b;
    if (r > INT32_MAX)
        return INT32_MAX;
    if (r < INT32_MIN)
        return INT32_MIN;
    return (fx_t)r;
}

/* a * b, with the Q16.16 rescale, truncating toward zero. */
fx_t fx_mul(fx_t a, fx_t b)
{
    /* FX_ONE is 1 << FX_SHIFT, so the rescale is the shift the format is
     * named for; take it directly and drop the 64-bit division. */
    int64_t r = ((int64_t)a * (int64_t)b) >> FX_SHIFT;
    if (r > INT32_MAX)
        return INT32_MAX;
    if (r < INT32_MIN)
        return INT32_MIN;
    return (fx_t)r;
}

/* a / b, with the Q16.16 rescale.  Division by zero saturates by sign. */
fx_t fx_div(fx_t a, fx_t b)
{
    int64_t r = 0;
    if (b == 0)
        return a >= 0 ? INT32_MAX : INT32_MIN;
    r = ((int64_t)a * FX_ONE) / (int64_t)b;
    if (r > INT32_MAX)
        return INT32_MAX;
    if (r < INT32_MIN)
        return INT32_MIN;
    return (fx_t)r;
}

/* Round x to the nearest whole number, halves away from zero. */
int32_t fx_round_to_int(fx_t x)
{
    int64_t v = x;
    if (v >= 0)
        return (int32_t)((v + (FX_ONE / 2)) >> FX_SHIFT);
    return -(int32_t)(((-v) + (FX_ONE / 2)) >> FX_SHIFT);
}

/* Constrain x to [lo, hi].  An inverted range answers lo. */
fx_t fx_clamp(fx_t x, fx_t lo, fx_t hi)
{
    if (lo > hi)
        return lo;
    if (x < lo)
        return lo;
    if (x > hi)
        return hi;
    return x;
}

/* Parse a decimal number with at most four fractional digits. */
int fx_from_str(const char *s, size_t n, fx_t *out)
{
    size_t i = 0;
    int negative = 0;
    int seen_digit = 0;
    int in_range = 1;
    int64_t whole = 0;
    int64_t frac = 0;
    int64_t scale = 1;

    if (s == NULL || out == NULL)
        return CFX_ERR_INPUT;
    *out = 0;

    if (i < n && (s[i] == '-' || s[i] == '+')) {
        negative = (s[i] == '-');
        i++;
    }
    for (; i < n && s[i] >= '0' && s[i] <= '9'; i++) {
        whole = (whole * 10) + (s[i] - '0');
        seen_digit = 1;
        if (whole > 32767)
            in_range = 0;
    }
    if (i < n && s[i] == '.') {
        i++;
        for (; i < n && s[i] >= '0' && s[i] <= '9' && scale < 10000; i++) {
            frac = (frac * 10) + (s[i] - '0');
            scale *= 10;
            seen_digit = 1;
        }
    }
    if (i != n)
        return CFX_ERR_FORMAT;
    if (!(seen_digit && in_range))
        return CFX_ERR_FORMAT;

    {
        int64_t v = (whole << FX_SHIFT) + ((frac << FX_SHIFT) / scale);
        *out = (fx_t)(negative ? -v : v);
    }
    return CFX_OK;
}
