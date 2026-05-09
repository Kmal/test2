#include "audio_metrics.h"

#include <stdio.h>
#include <stdlib.h>

#define ASSERT_TRUE(value) do { \
    if (!(value)) { \
        fprintf(stderr, "%s:%d assertion failed: %s\n", __FILE__, __LINE__, #value); \
        exit(1); \
    } \
} while (0)

#define ASSERT_EQ(expected, actual) do { \
    if ((expected) != (actual)) { \
        fprintf(stderr, "%s:%d expected %ld got %ld\n", __FILE__, __LINE__, (long)(expected), (long)(actual)); \
        exit(1); \
    } \
} while (0)

static void finalize_window(const int16_t *samples, size_t count, audio_level_metrics_t *out)
{
    audio_metrics_accumulator_t acc;
    audio_metrics_accumulator_init(&acc, 16000, 100);
    ASSERT_TRUE(audio_metrics_accumulator_add_i16(&acc, samples, count));
    ASSERT_TRUE(audio_metrics_accumulator_ready(&acc));
    ASSERT_TRUE(audio_metrics_accumulator_finalize(&acc, 7, out));
}

static void test_silence(void)
{
    int16_t samples[1600] = {0};
    audio_level_metrics_t metrics;
    finalize_window(samples, 1600, &metrics);
    ASSERT_EQ(AUDIO_METRICS_DBFS_Q8_SILENCE, metrics.rms_dbfs_q8);
    ASSERT_EQ(AUDIO_METRICS_DBFS_Q8_SILENCE, metrics.peak_dbfs_q8);
    ASSERT_EQ(0, metrics.rms_percent);
    ASSERT_EQ(0, metrics.peak_percent);
    ASSERT_EQ(0, metrics.vu_percent);
    ASSERT_EQ(0, metrics.clipped_samples);
    ASSERT_TRUE((metrics.flags & AUDIO_METRICS_FLAG_CLIPPING) == 0);
}

static void test_positive_clipping(void)
{
    int16_t samples[1600];
    for (size_t i = 0; i < 1600; ++i) {
        samples[i] = 32767;
    }
    audio_level_metrics_t metrics;
    finalize_window(samples, 1600, &metrics);
    ASSERT_EQ(1600, metrics.clipped_samples);
    ASSERT_TRUE((metrics.flags & AUDIO_METRICS_FLAG_CLIPPING) != 0);
    ASSERT_TRUE(metrics.peak_percent >= 99);
    ASSERT_TRUE(metrics.vu_percent >= 99);
}

static void test_negative_clipping(void)
{
    int16_t samples[1600];
    for (size_t i = 0; i < 1600; ++i) {
        samples[i] = -32768;
    }
    audio_level_metrics_t metrics;
    finalize_window(samples, 1600, &metrics);
    ASSERT_EQ(1600, metrics.clipped_samples);
    ASSERT_TRUE((metrics.flags & AUDIO_METRICS_FLAG_CLIPPING) != 0);
    ASSERT_EQ(100, metrics.peak_percent);
}

static void test_half_scale_square_wave(void)
{
    int16_t samples[1600];
    for (size_t i = 0; i < 1600; ++i) {
        samples[i] = (i % 2) ? 16384 : -16384;
    }
    audio_level_metrics_t metrics;
    finalize_window(samples, 1600, &metrics);
    ASSERT_TRUE(metrics.peak_dbfs_q8 <= (-5 * 256));
    ASSERT_TRUE(metrics.peak_dbfs_q8 >= (-7 * 256));
    ASSERT_TRUE(metrics.peak_percent > 80);
    ASSERT_TRUE((metrics.flags & AUDIO_METRICS_FLAG_CLIPPING) == 0);
}


static void test_custom_config_and_calibration(void)
{
    int16_t samples[1600];
    for (size_t i = 0; i < 1600; ++i) {
        samples[i] = (i % 2) ? 8192 : -8192;
    }
    audio_metrics_accumulator_t acc;
    audio_metrics_accumulator_init(&acc, 16000, 100);
    ASSERT_TRUE(audio_metrics_accumulator_add_i16(&acc, samples, 1600));
    audio_level_metrics_t metrics;
    audio_metrics_config_t config = {
        .floor_dbfs_q8 = -80 * 256,
        .ceiling_dbfs_q8 = 0,
        .loud_threshold_dbfs_q8 = -10 * 256,
    };
    ASSERT_TRUE(audio_metrics_accumulator_finalize_with_config(&acc, 2, &config, &metrics));
    ASSERT_TRUE(metrics.vu_percent > 70);
    ASSERT_TRUE((metrics.flags & AUDIO_METRICS_FLAG_LOUD) == 0);

    audio_calibration_t cal;
    audio_calibration_init(&cal);
    audio_calibration_begin(&cal, 2);
    audio_calibration_add_window(&cal, &metrics);
    ASSERT_TRUE(!audio_calibration_ready(&cal));
    audio_calibration_add_window(&cal, &metrics);
    ASSERT_TRUE(audio_calibration_ready(&cal));
    ASSERT_TRUE(audio_calibration_finalize(&cal));
    ASSERT_TRUE(cal.valid);
    ASSERT_TRUE(!cal.active);
    ASSERT_EQ(metrics.rms_dbfs_q8, cal.noise_floor_dbfs_q8);
    ASSERT_EQ(metrics.rms_dbfs_q8 + 6 * 256, cal.quiet_threshold_dbfs_q8);
    ASSERT_EQ(metrics.rms_dbfs_q8 + 25 * 256, cal.loud_threshold_dbfs_q8);
    audio_calibration_add_window(&cal, &metrics);
    ASSERT_TRUE(cal.valid);
    ASSERT_EQ(2, cal.collected_windows);
}

static void test_ready_boundary_and_reset(void)
{
    int16_t samples[1600] = {1};
    audio_metrics_accumulator_t acc;
    audio_metrics_accumulator_init(&acc, 16000, 100);
    ASSERT_TRUE(audio_metrics_accumulator_add_i16(&acc, samples, 1599));
    ASSERT_TRUE(!audio_metrics_accumulator_ready(&acc));
    ASSERT_TRUE(audio_metrics_accumulator_add_i16(&acc, samples + 1599, 1));
    ASSERT_TRUE(audio_metrics_accumulator_ready(&acc));
    audio_level_metrics_t metrics;
    ASSERT_TRUE(audio_metrics_accumulator_finalize(&acc, 1, &metrics));
    ASSERT_EQ(0, acc.collected_samples);
    ASSERT_EQ(0, acc.sum_squares);
    ASSERT_EQ(0, acc.clipped_samples);
}

int main(void)
{
    test_silence();
    test_positive_clipping();
    test_negative_clipping();
    test_half_scale_square_wave();
    test_ready_boundary_and_reset();
    test_custom_config_and_calibration();
    puts("audio_metrics tests passed");
    return 0;
}
