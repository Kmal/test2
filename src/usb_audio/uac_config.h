#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UAC_AUDIO_DEFAULT_SAMPLE_RATE_HZ 16000u
#define UAC_AUDIO_DEFAULT_RING_BUFFER_BYTES 16384u
#define UAC_AUDIO_SAFE_MAX_VOLUME_PERCENT 74u

typedef enum {
    UAC_AUDIO_MODE_DISABLED = 0,
    UAC_AUDIO_MODE_MIC_ONLY,
    UAC_AUDIO_MODE_SPEAKER_ONLY,
    UAC_AUDIO_MODE_COMBINED_SERIALIZED,
    UAC_AUDIO_MODE_SIMULTANEOUS_MIC_SPEAKER,
} uac_audio_mode_t;

typedef struct {
    bool uac_enabled;
    bool mic_enabled;
    bool speaker_enabled;
    bool combined_experimental;
    bool simultaneous_mic_speaker_enabled;
    uint32_t sample_rate_hz;
    size_t ring_buffer_bytes;
} uac_audio_config_t;

typedef struct {
    uac_audio_mode_t mode;
    uint32_t sample_rate_hz;
    uint8_t channels_per_direction;
    uint8_t bits_per_sample;
    size_t ring_buffer_bytes;
} uac_audio_runtime_config_t;

esp_err_t uac_audio_config_resolve(const uac_audio_config_t *config, uac_audio_runtime_config_t *out);
const char *uac_audio_mode_name(uac_audio_mode_t mode);
uint8_t uac_audio_clamp_volume_percent(uint8_t requested_percent);

#ifdef __cplusplus
}
#endif
