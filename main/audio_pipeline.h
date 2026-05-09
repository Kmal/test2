#pragma once

#include <stddef.h>
#include <stdint.h>

typedef size_t (*audio_pipeline_read_cb_t)(int16_t *dst, size_t samples, void *ctx);
typedef size_t (*audio_pipeline_write_cb_t)(const int16_t *src, size_t samples, void *ctx);

typedef struct {
    int sample_rate_hz;
    int channels;
    int bits_per_sample;
    audio_pipeline_read_cb_t read_cb;
    audio_pipeline_write_cb_t write_cb;
    void *ctx;
    uint32_t underruns;
    uint32_t overruns;
} audio_pipeline_t;

void audio_pipeline_init(audio_pipeline_t *pipeline, int sample_rate_hz,
                         int channels, int bits_per_sample,
                         audio_pipeline_read_cb_t read_cb,
                         audio_pipeline_write_cb_t write_cb,
                         void *ctx);
size_t audio_pipeline_capture_read(audio_pipeline_t *pipeline, int16_t *dst,
                                   size_t samples, int zero_fill);
size_t audio_pipeline_playback_write(audio_pipeline_t *pipeline,
                                     const int16_t *src, size_t samples);
uint32_t audio_pipeline_get_underruns(const audio_pipeline_t *pipeline);
uint32_t audio_pipeline_get_overruns(const audio_pipeline_t *pipeline);
