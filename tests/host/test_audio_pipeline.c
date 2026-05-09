#include "audio_pipeline.h"

#include <stdio.h>
#include <stdlib.h>

#define ASSERT_EQ(expected, actual) do { \
    if ((expected) != (actual)) { \
        fprintf(stderr, "%s:%d expected %ld got %ld\n", __FILE__, __LINE__, (long)(expected), (long)(actual)); \
        exit(1); \
    } \
} while (0)

typedef struct {
    const int16_t *input;
    size_t available;
    size_t write_limit;
} fake_io_t;

static size_t fake_read(int16_t *dst, size_t samples, void *ctx)
{
    fake_io_t *fake = (fake_io_t *)ctx;
    size_t n = fake->available < samples ? fake->available : samples;
    for (size_t i = 0; i < n; ++i) {
        dst[i] = fake->input[i];
    }
    return n;
}

static size_t fake_write(const int16_t *src, size_t samples, void *ctx)
{
    (void)src;
    fake_io_t *fake = (fake_io_t *)ctx;
    return fake->write_limit < samples ? fake->write_limit : samples;
}

static void test_full_capture_no_underrun(void)
{
    int16_t input[3] = {1, 2, 3};
    int16_t output[3] = {0};
    fake_io_t fake = {.input = input, .available = 3};
    audio_pipeline_t pipeline;
    audio_pipeline_init(&pipeline, 16000, 1, 16, fake_read, fake_write, &fake);
    ASSERT_EQ(3, audio_pipeline_capture_read(&pipeline, output, 3, 1));
    ASSERT_EQ(0, audio_pipeline_get_underruns(&pipeline));
    ASSERT_EQ(3, output[2]);
}

static void test_partial_capture_zero_fills_and_counts_underrun(void)
{
    int16_t input[1] = {7};
    int16_t output[3] = {9, 9, 9};
    fake_io_t fake = {.input = input, .available = 1};
    audio_pipeline_t pipeline;
    audio_pipeline_init(&pipeline, 16000, 1, 16, fake_read, fake_write, &fake);
    ASSERT_EQ(3, audio_pipeline_capture_read(&pipeline, output, 3, 1));
    ASSERT_EQ(1, audio_pipeline_get_underruns(&pipeline));
    ASSERT_EQ(7, output[0]);
    ASSERT_EQ(0, output[1]);
    ASSERT_EQ(0, output[2]);
}

static void test_partial_write_counts_overrun(void)
{
    int16_t input[3] = {1, 2, 3};
    fake_io_t fake = {.input = input, .available = 3, .write_limit = 2};
    audio_pipeline_t pipeline;
    audio_pipeline_init(&pipeline, 16000, 1, 16, fake_read, fake_write, &fake);
    ASSERT_EQ(2, audio_pipeline_playback_write(&pipeline, input, 3));
    ASSERT_EQ(1, audio_pipeline_get_overruns(&pipeline));
}

int main(void)
{
    test_full_capture_no_underrun();
    test_partial_capture_zero_fills_and_counts_underrun();
    test_partial_write_counts_overrun();
    puts("audio_pipeline tests passed");
    return 0;
}
