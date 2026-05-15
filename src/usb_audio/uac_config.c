#include "uac_config.h"

static bool uac_audio_sample_rate_supported(uint32_t sample_rate_hz)
{
    return sample_rate_hz == UAC_AUDIO_DEFAULT_SAMPLE_RATE_HZ;
}

esp_err_t uac_audio_config_resolve(const uac_audio_config_t *config, uac_audio_runtime_config_t *out)
{
    if (config == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!config->uac_enabled) {
        *out = (uac_audio_runtime_config_t){.mode = UAC_AUDIO_MODE_DISABLED};
        return ESP_OK;
    }
    if (!config->mic_enabled && !config->speaker_enabled) {
        return ESP_ERR_INVALID_ARG;
    }
    const uint32_t rate = config->sample_rate_hz == 0 ? UAC_AUDIO_DEFAULT_SAMPLE_RATE_HZ : config->sample_rate_hz;
    if (!uac_audio_sample_rate_supported(rate)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    const size_t ring_bytes = config->ring_buffer_bytes == 0 ? UAC_AUDIO_DEFAULT_RING_BUFFER_BYTES : config->ring_buffer_bytes;
    if (ring_bytes < 4096u) {
        return ESP_ERR_INVALID_ARG;
    }

    uac_audio_mode_t mode = UAC_AUDIO_MODE_DISABLED;
    if (config->mic_enabled && config->speaker_enabled) {
        if (config->simultaneous_mic_speaker_enabled && config->combined_experimental) {
            mode = UAC_AUDIO_MODE_SIMULTANEOUS_MIC_SPEAKER;
        } else if (config->simultaneous_mic_speaker_enabled) {
            return ESP_ERR_INVALID_STATE;
        } else if (config->combined_experimental) {
            mode = UAC_AUDIO_MODE_COMBINED_SERIALIZED;
        } else {
            return ESP_ERR_INVALID_STATE;
        }
    } else if (config->mic_enabled) {
        mode = UAC_AUDIO_MODE_MIC_ONLY;
    } else {
        mode = UAC_AUDIO_MODE_SPEAKER_ONLY;
    }

    *out = (uac_audio_runtime_config_t){
        .mode = mode,
        .sample_rate_hz = rate,
        .channels_per_direction = 1,
        .bits_per_sample = 16,
        .ring_buffer_bytes = ring_bytes,
    };
    return ESP_OK;
}

const char *uac_audio_mode_name(uac_audio_mode_t mode)
{
    switch (mode) {
    case UAC_AUDIO_MODE_DISABLED: return "disabled";
    case UAC_AUDIO_MODE_MIC_ONLY: return "mic-only";
    case UAC_AUDIO_MODE_SPEAKER_ONLY: return "speaker-only";
    case UAC_AUDIO_MODE_COMBINED_SERIALIZED: return "combined-serialized";
    case UAC_AUDIO_MODE_SIMULTANEOUS_MIC_SPEAKER: return "simultaneous-mic-speaker";
    default: return "unknown";
    }
}

uint8_t uac_audio_clamp_volume_percent(uint8_t requested_percent)
{
    return requested_percent > UAC_AUDIO_SAFE_MAX_VOLUME_PERCENT ? UAC_AUDIO_SAFE_MAX_VOLUME_PERCENT : requested_percent;
}
