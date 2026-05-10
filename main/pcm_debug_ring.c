#include "pcm_debug_ring.h"

#include <string.h>

void pcm_debug_ring_init(pcm_debug_ring_t *ring)
{
    if (ring == NULL) {
        return;
    }
    memset(ring, 0, sizeof(*ring));
}

void pcm_debug_ring_clear(pcm_debug_ring_t *ring)
{
    if (ring == NULL) {
        return;
    }
    ring->write_pos = 0;
    ring->read_pos = 0;
    ring->used = 0;
}

static void advance_pos(size_t *pos, size_t count)
{
    *pos = (*pos + count) % PCM_DEBUG_RING_BYTES;
}

size_t pcm_debug_ring_write(pcm_debug_ring_t *ring, const uint8_t *src, size_t bytes)
{
    if (ring == NULL || src == NULL || bytes == 0) {
        return 0;
    }

    size_t written = 0;
    while (written < bytes) {
        if (ring->used == PCM_DEBUG_RING_BYTES) {
            advance_pos(&ring->read_pos, 1);
            ring->used--;
            if (ring->overruns != UINT32_MAX) {
                ring->overruns++;
            }
        }

        size_t contiguous = PCM_DEBUG_RING_BYTES - ring->write_pos;
        size_t available = PCM_DEBUG_RING_BYTES - ring->used;
        size_t chunk = bytes - written;
        if (chunk > contiguous) {
            chunk = contiguous;
        }
        if (chunk > available) {
            chunk = available;
        }
        if (chunk == 0) {
            continue;
        }
        memcpy(&ring->buffer[ring->write_pos], &src[written], chunk);
        advance_pos(&ring->write_pos, chunk);
        ring->used += chunk;
        written += chunk;
    }
    return written;
}

size_t pcm_debug_ring_read(pcm_debug_ring_t *ring, uint8_t *dst, size_t max_bytes)
{
    if (ring == NULL || dst == NULL || max_bytes == 0 || ring->used == 0) {
        return 0;
    }

    size_t to_read = max_bytes < ring->used ? max_bytes : ring->used;
    size_t read = 0;
    while (read < to_read) {
        size_t contiguous = PCM_DEBUG_RING_BYTES - ring->read_pos;
        size_t chunk = to_read - read;
        if (chunk > contiguous) {
            chunk = contiguous;
        }
        memcpy(&dst[read], &ring->buffer[ring->read_pos], chunk);
        advance_pos(&ring->read_pos, chunk);
        ring->used -= chunk;
        read += chunk;
    }
    return read;
}

uint32_t pcm_debug_ring_get_overruns(const pcm_debug_ring_t *ring)
{
    return ring == NULL ? 0 : ring->overruns;
}
