#include "audio_pipeline.h"

#include <string.h>

void audio_pipeline_init(audio_pipeline_t *pipeline, int sample_rate_hz,
                         int channels, int bits_per_sample,
                         audio_pipeline_read_cb_t read_cb,
                         audio_pipeline_write_cb_t write_cb,
                         void *ctx)
{
    pipeline->sample_rate_hz = sample_rate_hz;
    pipeline->channels = channels;
    pipeline->bits_per_sample = bits_per_sample;
    pipeline->read_cb = read_cb;
    pipeline->write_cb = write_cb;
    pipeline->ctx = ctx;
    pipeline->underruns = 0;
    pipeline->overruns = 0;
}

size_t audio_pipeline_capture_read(audio_pipeline_t *pipeline, int16_t *dst,
                                   size_t samples, int zero_fill)
{
    size_t read = 0;
    if (pipeline->read_cb != NULL) {
        read = pipeline->read_cb(dst, samples, pipeline->ctx);
    }
    if (read < samples) {
        pipeline->underruns++;
        if (zero_fill) {
            memset(dst + read, 0, (samples - read) * sizeof(dst[0]));
            return samples;
        }
    }
    return read;
}

size_t audio_pipeline_playback_write(audio_pipeline_t *pipeline,
                                     const int16_t *src, size_t samples)
{
    if (pipeline->write_cb == NULL) {
        pipeline->overruns++;
        return 0;
    }
    size_t written = pipeline->write_cb(src, samples, pipeline->ctx);
    if (written < samples) {
        pipeline->overruns++;
    }
    return written;
}

uint32_t audio_pipeline_get_underruns(const audio_pipeline_t *pipeline)
{
    return pipeline->underruns;
}

uint32_t audio_pipeline_get_overruns(const audio_pipeline_t *pipeline)
{
    return pipeline->overruns;
}
