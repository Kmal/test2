#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t starts;
    uint32_t stops;
    uint32_t read_calls;
    uint32_t read_failures;
    uint32_t samples_read;
} uac_mic_source_stats_t;

typedef struct {
    esp_err_t (*init_capture)(void *ctx);
    esp_err_t (*read_i16)(int16_t *dest, size_t max_samples, size_t *samples_read, uint32_t timeout_ms, void *ctx);
    esp_err_t (*deinit)(void *ctx);
    void *ctx;
} uac_mic_source_ops_t;

esp_err_t uac_mic_source_init(const uac_mic_source_ops_t *ops);
esp_err_t uac_mic_source_start(void);
esp_err_t uac_mic_source_read_i16(int16_t *dest, size_t max_samples, size_t *samples_read, uint32_t timeout_ms);
esp_err_t uac_mic_source_stop(void);
uac_mic_source_stats_t uac_mic_source_get_stats(void);
void uac_mic_source_reset_for_test(void);

#ifdef __cplusplus
}
#endif
