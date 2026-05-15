#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "uac_audio_buffer.h"
#include "uac_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uac_audio_runtime_config_t runtime;
    uac_audio_buffer_t *mic_buffer;
    uac_audio_buffer_t *speaker_buffer;
    uint8_t volume_percent;
    bool muted;
} uac_device_adapter_t;

typedef struct {
    bool input_enabled;
    bool output_enabled;
    bool skip_tinyusb_init;
    const char *product_name;
    uint32_t sample_rate_hz;
    uint8_t channels_per_direction;
    uint8_t bits_per_sample;
} uac_device_descriptor_plan_t;

esp_err_t uac_device_adapter_init(uac_device_adapter_t *adapter,
                                  const uac_audio_runtime_config_t *runtime,
                                  uac_audio_buffer_t *mic_buffer,
                                  uac_audio_buffer_t *speaker_buffer);
uac_device_descriptor_plan_t uac_device_adapter_descriptor_plan(const uac_device_adapter_t *adapter);
esp_err_t uac_device_adapter_input_cb(uint8_t *buf, size_t len, size_t *bytes_read, void *ctx);
esp_err_t uac_device_adapter_output_cb(uint8_t *buf, size_t len, void *ctx);
void uac_device_adapter_set_mute_cb(uint32_t mute, void *ctx);
void uac_device_adapter_set_volume_cb(uint32_t volume, void *ctx);

#ifdef __cplusplus
}
#endif
