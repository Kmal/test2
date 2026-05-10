#include "audio_metrics.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define AUDIO_METRICS_FULL_SCALE 32768U
#define AUDIO_METRICS_DEFAULT_FLOOR_Q8 (-60 * 256)
#define AUDIO_METRICS_DEFAULT_CEILING_Q8 0
#define AUDIO_METRICS_LOUD_Q8 (-12 * 256)
#define AUDIO_METRICS_VOICE_FLOOR_Q8 (-50 * 256)

static uint32_t isqrt_u64(uint64_t value)
{
    uint64_t op = value;
    uint64_t res = 0;
    uint64_t one = (uint64_t)1 << 62;
    while (one > op) {
        one >>= 2;
    }
    while (one != 0) {
        if (op >= res + one) {
            op -= res + one;
            res += 2 * one;
        }
        res >>= 1;
        one >>= 2;
    }
    return (uint32_t)res;
}

void audio_metrics_accumulator_reset(audio_metrics_accumulator_t *acc)
{
    if (acc == NULL) {
        return;
    }
    uint32_t sample_rate_hz = acc->sample_rate_hz;
    uint32_t target_window_ms = acc->target_window_ms;
    uint32_t target_samples = acc->target_samples;
    memset(acc, 0, sizeof(*acc));
    acc->sample_rate_hz = sample_rate_hz;
    acc->target_window_ms = target_window_ms;
    acc->target_samples = target_samples;
}

void audio_metrics_accumulator_init(audio_metrics_accumulator_t *acc,
                                    uint32_t sample_rate_hz,
                                    uint32_t window_ms)
{
    if (acc == NULL) {
        return;
    }
    if (sample_rate_hz == 0) {
        sample_rate_hz = AUDIO_METRICS_DEFAULT_SAMPLE_RATE_HZ;
    }
    if (window_ms == 0) {
        window_ms = AUDIO_METRICS_DEFAULT_WINDOW_MS;
    }
    memset(acc, 0, sizeof(*acc));
    acc->sample_rate_hz = sample_rate_hz;
    acc->target_window_ms = window_ms;
    acc->target_samples = (sample_rate_hz * window_ms) / 1000U;
    if (acc->target_samples == 0) {
        acc->target_samples = 1;
    }
}

bool audio_metrics_accumulator_add_i16(audio_metrics_accumulator_t *acc,
                                       const int16_t *samples,
                                       size_t sample_count)
{
    if (acc == NULL || samples == NULL) {
        return false;
    }
    for (size_t i = 0; i < sample_count; ++i) {
        int32_t sample = samples[i];
        uint32_t abs_sample = sample == AUDIO_METRICS_CLIP_NEGATIVE
                                  ? AUDIO_METRICS_FULL_SCALE
                                  : (uint32_t)abs(sample);
        acc->sum_squares += (uint64_t)abs_sample * abs_sample;
        acc->sum_abs += abs_sample;
        if (abs_sample > acc->peak_abs) {
            acc->peak_abs = abs_sample;
            acc->peak_sample = samples[i];
        }
        if (samples[i] == AUDIO_METRICS_CLIP_POSITIVE || samples[i] == AUDIO_METRICS_CLIP_NEGATIVE) {
            if (acc->clipped_samples != UINT16_MAX) {
                acc->clipped_samples++;
            }
        }
        if (acc->has_previous_sample &&
            ((acc->previous_sample < 0 && samples[i] >= 0) ||
             (acc->previous_sample >= 0 && samples[i] < 0))) {
            acc->zero_crossings++;
        }
        acc->previous_sample = samples[i];
        acc->has_previous_sample = true;
        acc->collected_samples++;
    }
    return sample_count > 0;
}

bool audio_metrics_accumulator_ready(const audio_metrics_accumulator_t *acc)
{
    return acc != NULL && acc->collected_samples >= acc->target_samples;
}

int32_t audio_metrics_amplitude_to_dbfs_q8(uint32_t amplitude)
{
    if (amplitude == 0) {
        return AUDIO_METRICS_DBFS_Q8_SILENCE;
    }
    if (amplitude >= AUDIO_METRICS_FULL_SCALE) {
        return 0;
    }
    double ratio = (double)amplitude / (double)AUDIO_METRICS_FULL_SCALE;
    int32_t q8 = (int32_t)lround(20.0 * log10(ratio) * 256.0);
    if (q8 < AUDIO_METRICS_DBFS_Q8_SILENCE) {
        return AUDIO_METRICS_DBFS_Q8_SILENCE;
    }
    if (q8 > 0) {
        return 0;
    }
    return q8;
}

uint16_t audio_metrics_dbfs_q8_to_percent(int32_t dbfs_q8,
                                          int32_t floor_dbfs_q8,
                                          int32_t ceiling_dbfs_q8)
{
    if (ceiling_dbfs_q8 <= floor_dbfs_q8) {
        return 0;
    }
    if (dbfs_q8 <= floor_dbfs_q8) {
        return 0;
    }
    if (dbfs_q8 >= ceiling_dbfs_q8) {
        return 100;
    }
    int32_t num = (dbfs_q8 - floor_dbfs_q8) * 100;
    int32_t den = ceiling_dbfs_q8 - floor_dbfs_q8;
    return (uint16_t)(num / den);
}

bool audio_metrics_accumulator_finalize_with_config(audio_metrics_accumulator_t *acc,
                                                   uint32_t sequence,
                                                   const audio_metrics_config_t *config,
                                                   audio_level_metrics_t *out)
{
    if (acc == NULL || out == NULL || acc->collected_samples == 0) {
        return false;
    }

    audio_metrics_config_t effective = {
        .floor_dbfs_q8 = AUDIO_METRICS_DEFAULT_FLOOR_Q8,
        .ceiling_dbfs_q8 = AUDIO_METRICS_DEFAULT_CEILING_Q8,
        .loud_threshold_dbfs_q8 = AUDIO_METRICS_LOUD_Q8,
    };
    if (config != NULL) {
        effective = *config;
    }

    uint32_t samples = acc->collected_samples;
    uint64_t mean_square = acc->sum_squares / samples;
    uint32_t rms = isqrt_u64(mean_square);
    uint32_t peak = acc->peak_abs;

    memset(out, 0, sizeof(*out));
    out->sequence = sequence;
    out->sample_rate_hz = acc->sample_rate_hz;
    out->window_ms = (samples * 1000U) / acc->sample_rate_hz;
    out->samples = samples;
    out->rms_dbfs_q8 = audio_metrics_amplitude_to_dbfs_q8(rms);
    out->peak_dbfs_q8 = audio_metrics_amplitude_to_dbfs_q8(peak);
    out->rms_percent = audio_metrics_dbfs_q8_to_percent(out->rms_dbfs_q8,
                                                        effective.floor_dbfs_q8,
                                                        effective.ceiling_dbfs_q8);
    out->peak_percent = audio_metrics_dbfs_q8_to_percent(out->peak_dbfs_q8,
                                                         effective.floor_dbfs_q8,
                                                         effective.ceiling_dbfs_q8);
    out->vu_percent = out->rms_percent;
    out->peak_sample = acc->peak_sample;
    out->clipped_samples = acc->clipped_samples;
    out->zero_crossings = acc->zero_crossings;
    if (acc->clipped_samples > 0) {
        out->flags |= AUDIO_METRICS_FLAG_CLIPPING;
    }
    if (out->rms_dbfs_q8 >= effective.loud_threshold_dbfs_q8) {
        out->flags |= AUDIO_METRICS_FLAG_LOUD;
    }
    if (out->rms_dbfs_q8 >= AUDIO_METRICS_VOICE_FLOOR_Q8 && out->zero_crossings > samples / 100U) {
        out->flags |= AUDIO_METRICS_FLAG_VOICE_LIKE;
    }
    audio_metrics_accumulator_reset(acc);
    return true;
}

bool audio_metrics_accumulator_finalize(audio_metrics_accumulator_t *acc,
                                        uint32_t sequence,
                                        audio_level_metrics_t *out)
{
    return audio_metrics_accumulator_finalize_with_config(acc, sequence, NULL, out);
}

void audio_calibration_init(audio_calibration_t *cal)
{
    if (cal == NULL) {
        return;
    }
    memset(cal, 0, sizeof(*cal));
}

void audio_calibration_begin(audio_calibration_t *cal, uint32_t required_windows)
{
    if (cal == NULL) {
        return;
    }
    memset(cal, 0, sizeof(*cal));
    cal->active = true;
    cal->required_windows = required_windows == 0 ? 1 : required_windows;
}

void audio_calibration_add_window(audio_calibration_t *cal, const audio_level_metrics_t *metrics)
{
    if (cal == NULL || metrics == NULL || !cal->active || audio_calibration_ready(cal)) {
        return;
    }
    cal->sum_rms_dbfs_q8 += metrics->rms_dbfs_q8;
    cal->collected_windows++;
}

bool audio_calibration_ready(const audio_calibration_t *cal)
{
    return cal != NULL && cal->active && cal->collected_windows >= cal->required_windows;
}

bool audio_calibration_finalize(audio_calibration_t *cal)
{
    if (!audio_calibration_ready(cal) || cal->collected_windows == 0) {
        return false;
    }
    cal->noise_floor_dbfs_q8 = (int32_t)(cal->sum_rms_dbfs_q8 / (int64_t)cal->collected_windows);
    cal->quiet_threshold_dbfs_q8 = cal->noise_floor_dbfs_q8 + (6 * 256);
    cal->loud_threshold_dbfs_q8 = cal->noise_floor_dbfs_q8 + (25 * 256);
    cal->active = false;
    cal->valid = true;
    return true;
}
