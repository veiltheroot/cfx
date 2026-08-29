/* A fixed-capacity byte ring.  The storage is inline: nothing here allocates. */
#include "cfx_internal.h"

/* Reset a ring to empty. */
void ring_init(struct cfx_ring *r)
{
    unsigned i = 0;
    if (r == NULL)
        return;
    for (i = 0; i < CFX_RING_CAP; i++)
        r->data[i] = 0;
    r->head = 0;
    r->count = 0;
}

/* Append one byte.  A full ring is an error, not an overwrite. */
int ring_push_byte(struct cfx_ring *r, uint8_t b)
{
    uint32_t slot = 0;

    if (r == NULL)
        return CFX_ERR_INPUT;
    if (r->count >= CFX_RING_CAP)
        return CFX_ERR_SPACE;

    slot = (r->head + r->count) % CFX_RING_CAP;
    r->data[slot] = b;
    r->count++;
    return CFX_OK;
}

/* Remove up to *want* bytes from the front.  *got* is how many were moved. */
int ring_pop_bytes(struct cfx_ring *r, uint8_t *out, size_t want, size_t *got)
{
    size_t moved = 0;

    if (r == NULL || out == NULL || got == NULL)
        return CFX_ERR_INPUT;
    *got = 0;

    while (moved < want && r->count > 0) {
        out[moved] = r->data[r->head];
        r->head = (r->head + 1) % CFX_RING_CAP;
        r->count--;
        moved++;
    }
    *got = moved;
    return CFX_OK;
}

/* Hand back a pointer to n contiguous free bytes at the tail, or NULL.
 *
 * The run must not wrap, so a request that would straddle the end of the
 * storage is refused even when the ring has room overall. */
int ring_reserve(struct cfx_ring *r, size_t n, uint8_t **slot_out)
{
    uint32_t tail = 0;

    if (r == NULL || slot_out == NULL)
        return CFX_ERR_INPUT;
    *slot_out = NULL;

    /* An empty reservation is not a failure: it consumes nothing and
     * hands back the position the next byte would occupy. */
    if (n > CFX_RING_CAP - r->count)
        return CFX_ERR_SPACE;

    tail = (r->head + r->count) % CFX_RING_CAP;
    if (tail + n > CFX_RING_CAP)
        return CFX_ERR_SPACE;

    *slot_out = &r->data[tail];
    r->count += (uint32_t)n;
    return CFX_OK;
}
