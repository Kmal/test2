#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_METRICS_DBFS_Q8_SILENCE (-120 * 256)
#define AUDIO_METRICS_CLIP_POSITIVE 32767
#define AUDIO_METRICS_CLIP_NEGATIVE (-32768)
#define AUDIO_METRICS_DEFAULT_WINDOW_MS 100
#define AUDIO_METRICS_DEFAULT_SAMPLE_RATE_HZ 16000

typedef enum {
    AUDIO_METRICS_FLAG_NONE = 0,
    AUDIO_METRICS_FLAG_CLIPPING = 1u << 0,
    AUDIO_METRICS_FLAG_VOICE_LIKE = 1u << 1,
    AUDIO_METRICS_FLAG_LOUD = 1u << 2,
    AUDIO_METRICS_FLAG_UNDERRUN = 1u << 3,
} audio_metrics_flags_t;

typedef struct {
    uint32_t sample_rate_hz;
    uint32_t target_window_ms;
    uint32_t target_samples;
    uint32_t collected_samples;
    uint64_t sum_squares;
    uint64_t sum_abs;
    uint32_t peak_abs;
    int16_t peak_sample;
    uint16_t clipped_samples;
    uint32_t zero_crossings;
    int16_t previous_sample;
    bool has_previous_sample;
} audio_metrics_accumulator_t;

typedef struct {
    int32_t floor_dbfs_q8;
    int32_t ceiling_dbfs_q8;
    int32_t loud_threshold_dbfs_q8;
} audio_metrics_config_t;

typedef struct {
    bool active;
    bool valid;
    uint32_t required_windows;
    uint32_t collected_windows;
    int64_t sum_rms_dbfs_q8;
    int32_t noise_floor_dbfs_q8;
    int32_t quiet_threshold_dbfs_q8;
    int32_t loud_threshold_dbfs_q8;
} audio_calibration_t;

typedef struct {
    uint32_t sequence;
    uint32_t sample_rate_hz;
    uint32_t window_ms;
    uint32_t samples;
    int32_t rms_dbfs_q8;
    int32_t peak_dbfs_q8;
    uint16_t rms_percent;
    uint16_t peak_percent;
    uint16_t vu_percent;
    int16_t peak_sample;
    uint16_t clipped_samples;
    uint32_t zero_crossings;
    uint32_t flags;
} audio_level_metrics_t;

void audio_metrics_accumulator_init(audio_metrics_accumulator_t *acc,
                                    uint32_t sample_rate_hz,
                                    uint32_t window_ms);
void audio_metrics_accumulator_reset(audio_metrics_accumulator_t *acc);
bool audio_metrics_accumulator_add_i16(audio_metrics_accumulator_t *acc,
                                       const int16_t *samples,
                                       size_t sample_count);
bool audio_metrics_accumulator_ready(const audio_metrics_accumulator_t *acc);
bool audio_metrics_accumulator_finalize(audio_metrics_accumulator_t *acc,
                                        uint32_t sequence,
                                        audio_level_metrics_t *out);
bool audio_metrics_accumulator_finalize_with_config(audio_metrics_accumulator_t *acc,
                                                   uint32_t sequence,
                                                   const audio_metrics_config_t *config,
                                                   audio_level_metrics_t *out);
void audio_calibration_init(audio_calibration_t *cal);
void audio_calibration_begin(audio_calibration_t *cal, uint32_t required_windows);
void audio_calibration_add_window(audio_calibration_t *cal, const audio_level_metrics_t *metrics);
bool audio_calibration_ready(const audio_calibration_t *cal);
bool audio_calibration_finalize(audio_calibration_t *cal);
int32_t audio_metrics_amplitude_to_dbfs_q8(uint32_t amplitude);
uint16_t audio_metrics_dbfs_q8_to_percent(int32_t dbfs_q8,
                                          int32_t floor_dbfs_q8,
                                          int32_t ceiling_dbfs_q8);

#ifdef __cplusplus
}
#endif
