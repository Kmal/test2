#include "uac_device_adapter.h"

#include <string.h>

esp_err_t uac_device_adapter_init(uac_device_adapter_t *adapter,
                                  const uac_audio_runtime_config_t *runtime,
                                  uac_audio_buffer_t *mic_buffer,
                                  uac_audio_buffer_t *speaker_buffer)
{
    if (adapter == NULL || runtime == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const bool needs_mic = runtime->mode == UAC_AUDIO_MODE_MIC_ONLY ||
                           runtime->mode == UAC_AUDIO_MODE_COMBINED_SERIALIZED ||
                           runtime->mode == UAC_AUDIO_MODE_SIMULTANEOUS_MIC_SPEAKER;
    const bool needs_speaker = runtime->mode == UAC_AUDIO_MODE_SPEAKER_ONLY ||
                               runtime->mode == UAC_AUDIO_MODE_COMBINED_SERIALIZED ||
                               runtime->mode == UAC_AUDIO_MODE_SIMULTANEOUS_MIC_SPEAKER;
    if ((needs_mic && mic_buffer == NULL) || (needs_speaker && speaker_buffer == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }
    *adapter = (uac_device_adapter_t){
        .runtime = *runtime,
        .mic_buffer = mic_buffer,
        .speaker_buffer = speaker_buffer,
        .volume_percent = UAC_AUDIO_SAFE_MAX_VOLUME_PERCENT,
        .muted = false,
    };
    return ESP_OK;
}

uac_device_descriptor_plan_t uac_device_adapter_descriptor_plan(const uac_device_adapter_t *adapter)
{
    if (adapter == NULL) {
        return (uac_device_descriptor_plan_t){0};
    }
    const uac_audio_mode_t mode = adapter->runtime.mode;
    const bool input = mode == UAC_AUDIO_MODE_MIC_ONLY || mode == UAC_AUDIO_MODE_COMBINED_SERIALIZED ||
                       mode == UAC_AUDIO_MODE_SIMULTANEOUS_MIC_SPEAKER;
    const bool output = mode == UAC_AUDIO_MODE_SPEAKER_ONLY || mode == UAC_AUDIO_MODE_COMBINED_SERIALIZED ||
                        mode == UAC_AUDIO_MODE_SIMULTANEOUS_MIC_SPEAKER;
    const char *name = "StickS3 USB Audio";
    if (mode == UAC_AUDIO_MODE_MIC_ONLY) {
        name = "StickS3 USB Microphone";
    } else if (mode == UAC_AUDIO_MODE_SPEAKER_ONLY) {
        name = "StickS3 USB Speaker";
    }
    return (uac_device_descriptor_plan_t){
        .input_enabled = input,
        .output_enabled = output,
        .skip_tinyusb_init = false,
        .product_name = name,
        .sample_rate_hz = adapter->runtime.sample_rate_hz,
        .channels_per_direction = adapter->runtime.channels_per_direction,
        .bits_per_sample = adapter->runtime.bits_per_sample,
    };
}

esp_err_t uac_device_adapter_input_cb(uint8_t *buf, size_t len, size_t *bytes_read, void *ctx)
{
    if (buf == NULL || bytes_read == NULL || ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uac_device_adapter_t *adapter = (uac_device_adapter_t *)ctx;
    if (adapter->mic_buffer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    *bytes_read = uac_audio_buffer_read_or_silence(adapter->mic_buffer, buf, len);
    return ESP_OK;
}

esp_err_t uac_device_adapter_output_cb(uint8_t *buf, size_t len, void *ctx)
{
    if (buf == NULL || ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uac_device_adapter_t *adapter = (uac_device_adapter_t *)ctx;
    if (adapter->speaker_buffer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    size_t written = uac_audio_buffer_write(adapter->speaker_buffer, buf, len);
    return written == len ? ESP_OK : ESP_ERR_TIMEOUT;
}

void uac_device_adapter_set_mute_cb(uint32_t mute, void *ctx)
{
    if (ctx == NULL) {
        return;
    }
    uac_device_adapter_t *adapter = (uac_device_adapter_t *)ctx;
    adapter->muted = mute != 0;
}

void uac_device_adapter_set_volume_cb(uint32_t volume, void *ctx)
{
    if (ctx == NULL) {
        return;
    }
    uac_device_adapter_t *adapter = (uac_device_adapter_t *)ctx;
    adapter->volume_percent = uac_audio_clamp_volume_percent((uint8_t)volume);
}
