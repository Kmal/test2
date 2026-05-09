#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PCM_DEBUG_RING_BYTES 4096

typedef struct {
    uint8_t buffer[PCM_DEBUG_RING_BYTES];
    size_t write_pos;
    size_t read_pos;
    size_t used;
    uint32_t overruns;
} pcm_debug_ring_t;

void pcm_debug_ring_init(pcm_debug_ring_t *ring);
size_t pcm_debug_ring_write(pcm_debug_ring_t *ring, const uint8_t *src, size_t bytes);
size_t pcm_debug_ring_read(pcm_debug_ring_t *ring, uint8_t *dst, size_t max_bytes);
void pcm_debug_ring_clear(pcm_debug_ring_t *ring);
uint32_t pcm_debug_ring_get_overruns(const pcm_debug_ring_t *ring);

#ifdef __cplusplus
}
#endif
