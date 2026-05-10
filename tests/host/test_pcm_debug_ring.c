#include "pcm_debug_ring.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT_EQ(expected, actual) do { \
    if ((expected) != (actual)) { \
        fprintf(stderr, "%s:%d expected %ld got %ld\n", __FILE__, __LINE__, (long)(expected), (long)(actual)); \
        exit(1); \
    } \
} while (0)

static void test_empty_read(void)
{
    pcm_debug_ring_t ring;
    uint8_t out[4];
    pcm_debug_ring_init(&ring);
    ASSERT_EQ(0, pcm_debug_ring_read(&ring, out, sizeof(out)));
}

static void test_write_read_order(void)
{
    pcm_debug_ring_t ring;
    uint8_t input[4] = {1, 2, 3, 4};
    uint8_t out[4] = {0};
    pcm_debug_ring_init(&ring);
    ASSERT_EQ(4, pcm_debug_ring_write(&ring, input, sizeof(input)));
    ASSERT_EQ(4, pcm_debug_ring_read(&ring, out, sizeof(out)));
    ASSERT_EQ(0, memcmp(input, out, sizeof(input)));
}

static void test_wrap_and_overrun(void)
{
    pcm_debug_ring_t ring;
    pcm_debug_ring_init(&ring);
    uint8_t input[PCM_DEBUG_RING_BYTES + 3];
    for (size_t i = 0; i < sizeof(input); ++i) {
        input[i] = (uint8_t)i;
    }
    ASSERT_EQ(sizeof(input), pcm_debug_ring_write(&ring, input, sizeof(input)));
    ASSERT_EQ(3, pcm_debug_ring_get_overruns(&ring));
    uint8_t out[4];
    ASSERT_EQ(4, pcm_debug_ring_read(&ring, out, sizeof(out)));
    ASSERT_EQ(input[3], out[0]);
    ASSERT_EQ(input[6], out[3]);
}

static void test_clear(void)
{
    pcm_debug_ring_t ring;
    uint8_t input[3] = {9, 8, 7};
    uint8_t out[3];
    pcm_debug_ring_init(&ring);
    ASSERT_EQ(3, pcm_debug_ring_write(&ring, input, sizeof(input)));
    pcm_debug_ring_clear(&ring);
    ASSERT_EQ(0, pcm_debug_ring_read(&ring, out, sizeof(out)));
}

int main(void)
{
    test_empty_read();
    test_write_read_order();
    test_wrap_and_overrun();
    test_clear();
    puts("pcm_debug_ring tests passed");
    return 0;
}
