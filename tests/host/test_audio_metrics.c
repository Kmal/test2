#include "audio_metrics.h"

#include <stdio.h>
#include <stdlib.h>

#define ASSERT_TRUE(value) do { if (!(value)) { fprintf(stderr, "%s:%d assertion failed: %s\n", __FILE__, __LINE__, #value); exit(1); } } while (0)
#define ASSERT_EQ(expected, actual) do { if ((expected) != (actual)) { fprintf(stderr, "%s:%d expected %ld got %ld\n", __FILE__, __LINE__, (long)(expected), (long)(actual)); exit(1); } } while (0)
#define ASSERT_RANGE(value, low, high) do { long v__ = (long)(value); if (v__ < (long)(low) || v__ > (long)(high)) { fprintf(stderr, "%s:%d expected %s in [%ld,%ld], got %ld\n", __FILE__, __LINE__, #value, (long)(low), (long)(high), v__); exit(1); } } while (0)

static void test_amplitude_and_percent_helpers(void)
{
    ASSERT_EQ(AUDIO_METRICS_DBFS_Q8_SILENCE, audio_metrics_amplitude_to_dbfs_q8(0));
    ASSERT_EQ(0, audio_metrics_amplitude_to_dbfs_q8(32768));
    ASSERT_RANGE(audio_metrics_amplitude_to_dbfs_q8(16384), -1550, -1530);
    ASSERT_EQ(0, audio_metrics_dbfs_q8_to_percent(-60 * 256, -60 * 256, 0));
    ASSERT_EQ(50, audio_metrics_dbfs_q8_to_percent(-30 * 256, -60 * 256, 0));
    ASSERT_EQ(100, audio_metrics_dbfs_q8_to_percent(0, -60 * 256, 0));
    ASSERT_EQ(0, audio_metrics_dbfs_q8_to_percent(-30 * 256, 0, -60 * 256));
}

static void test_accumulator_finalize_resets_and_flags_window(void)
{
    audio_metrics_accumulator_t acc;
    audio_level_metrics_t metrics;
    const int16_t samples[] = {32767, -32768, 0, 0};

    audio_metrics_accumulator_init(&acc, 4000, 1);
    ASSERT_EQ(4, acc.target_samples);
    ASSERT_TRUE(!audio_metrics_accumulator_ready(&acc));
    ASSERT_TRUE(audio_metrics_accumulator_add_i16(&acc, samples, sizeof(samples) / sizeof(samples[0])));
    ASSERT_TRUE(audio_metrics_accumulator_ready(&acc));
    ASSERT_TRUE(audio_metrics_accumulator_finalize(&acc, 42, &metrics));

    ASSERT_EQ(42, metrics.sequence);
    ASSERT_EQ(4000, metrics.sample_rate_hz);
    ASSERT_EQ(1, metrics.window_ms);
    ASSERT_EQ(4, metrics.samples);
    ASSERT_EQ(-32768, metrics.peak_sample);
    ASSERT_EQ(2, metrics.clipped_samples);
    ASSERT_EQ(2, metrics.zero_crossings);
    ASSERT_TRUE((metrics.flags & AUDIO_METRICS_FLAG_CLIPPING) != 0u);
    ASSERT_TRUE((metrics.flags & AUDIO_METRICS_FLAG_LOUD) != 0u);
    ASSERT_TRUE((metrics.flags & AUDIO_METRICS_FLAG_VOICE_LIKE) != 0u);
    ASSERT_EQ(0, acc.collected_samples);
    ASSERT_EQ(4000, acc.sample_rate_hz);
    ASSERT_EQ(1, acc.target_window_ms);
    ASSERT_EQ(4, acc.target_samples);
}

static void test_calibration_thresholds(void)
{
    audio_calibration_t calibration;
    audio_level_metrics_t window = {0};

    audio_calibration_init(&calibration);
    audio_calibration_begin(&calibration, 2);
    window.rms_dbfs_q8 = -50 * 256;
    audio_calibration_add_window(&calibration, &window);
    ASSERT_TRUE(!audio_calibration_ready(&calibration));
    window.rms_dbfs_q8 = -40 * 256;
    audio_calibration_add_window(&calibration, &window);
    ASSERT_TRUE(audio_calibration_ready(&calibration));
    ASSERT_TRUE(audio_calibration_finalize(&calibration));
    ASSERT_TRUE(calibration.valid);
    ASSERT_TRUE(!calibration.active);
    ASSERT_EQ(-45 * 256, calibration.noise_floor_dbfs_q8);
    ASSERT_EQ(-39 * 256, calibration.quiet_threshold_dbfs_q8);
    ASSERT_EQ(-20 * 256, calibration.loud_threshold_dbfs_q8);
}

int main(void)
{
    test_amplitude_and_percent_helpers();
    test_accumulator_finalize_resets_and_flags_window();
    test_calibration_thresholds();
    puts("audio_metrics tests passed");
    return 0;
}
