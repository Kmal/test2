#include "sound_meter.h"

#include "board_i2s.h"
#include "board_sticks3.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "status_ui.h"
#include "transport_ble_gatt_pcm.h"

#include <string.h>

#define SOUND_METER_TASK_STACK 4096
#define SOUND_METER_TASK_PRIORITY 5
#define SOUND_METER_DEFAULT_I2S_TIMEOUT_MS 100

static const char *TAG = "SOUND_METER";

static sound_meter_config_t s_config;
static TaskHandle_t s_task;
static bool s_started;
static bool s_enabled;
static audio_level_metrics_t s_latest;
static bool s_latest_valid;
static sound_meter_stats_t s_stats;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

static app_runtime_state_t default_runtime(void)
{
    app_runtime_state_t state;
    app_runtime_state_init(&state);
    return state;
}

static app_runtime_state_t get_runtime_snapshot(void)
{
    if (s_config.get_runtime != NULL) {
        return s_config.get_runtime(s_config.runtime_ctx);
    }
    return default_runtime();
}

static void store_latest(const audio_level_metrics_t *metrics)
{
    portENTER_CRITICAL(&s_mux);
    s_latest = *metrics;
    s_latest_valid = true;
    s_stats.windows_completed++;
    s_stats.last_sequence = metrics->sequence;
    s_stats.last_update_ms = esp_timer_get_time() / 1000;
    portEXIT_CRITICAL(&s_mux);
}

static void increment_u32(uint32_t *value)
{
    if (*value != UINT32_MAX) {
        ++(*value);
    }
}

static void publish_ui_snapshot(const audio_level_metrics_t *metrics, const app_runtime_state_t *runtime)
{
    status_ui_sound_meter_snapshot_t snapshot = {
        .valid = true,
        .sequence = metrics->sequence,
        .age_ms = 0,
        .rms_dbfs_q8 = metrics->rms_dbfs_q8,
        .peak_dbfs_q8 = metrics->peak_dbfs_q8,
        .rms_percent = metrics->rms_percent,
        .peak_percent = metrics->peak_percent,
        .vu_percent = metrics->vu_percent,
        .clipped_samples = metrics->clipped_samples,
        .zero_crossings = metrics->zero_crossings,
        .flags = metrics->flags,
        .app_mode = (uint8_t)runtime->app_mode,
        .display_mode = (uint8_t)runtime->display_mode,
        .ble_connected = transport_ble_gatt_pcm_is_connected(),
        .ble_metrics_notify_enabled = transport_ble_gatt_pcm_metrics_notify_enabled(),
        .ble_pcm_notify_enabled = transport_ble_gatt_pcm_pcm_notify_enabled(),
    };
    status_ui_set_sound_meter_snapshot(&snapshot);
    portENTER_CRITICAL(&s_mux);
    increment_u32(&s_stats.lcd_updates);
    portEXIT_CRITICAL(&s_mux);
}

static void sound_meter_task(void *arg)
{
    (void)arg;
    uint8_t pcm_bytes[BOARD_PCM_CHUNK_SIZE];
    audio_metrics_accumulator_t acc;
    audio_metrics_accumulator_init(&acc, s_config.sample_rate_hz, s_config.metrics_window_ms);
    uint32_t sequence = 0;

    while (true) {
        app_runtime_state_t runtime = get_runtime_snapshot();
        if (!s_enabled || runtime.app_mode == APP_MODE_PAUSED) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        size_t bytes_read = 0;
        esp_err_t err = board_i2s_read(pcm_bytes, s_config.pcm_chunk_bytes, &bytes_read,
                                       pdMS_TO_TICKS(s_config.i2s_read_timeout_ms));
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "I2S read failed: %s", esp_err_to_name(err));
            portENTER_CRITICAL(&s_mux);
            increment_u32(&s_stats.i2s_read_errors);
            portEXIT_CRITICAL(&s_mux);
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        if (bytes_read == 0) {
            portENTER_CRITICAL(&s_mux);
            increment_u32(&s_stats.i2s_zero_reads);
            portEXIT_CRITICAL(&s_mux);
            continue;
        }

        audio_metrics_accumulator_add_i16(&acc, (const int16_t *)pcm_bytes, bytes_read / sizeof(int16_t));
        if (!audio_metrics_accumulator_ready(&acc)) {
            continue;
        }

        audio_level_metrics_t metrics;
        if (!audio_metrics_accumulator_finalize(&acc, sequence++, &metrics)) {
            continue;
        }
        store_latest(&metrics);
        if (s_config.enable_lcd_updates) {
            publish_ui_snapshot(&metrics, &runtime);
        }
        if (s_config.enable_ble_telemetry && runtime.ble_telemetry_enabled && runtime.app_mode != APP_MODE_PAUSED) {
            err = transport_ble_gatt_pcm_publish_metrics(&metrics, runtime.app_mode, runtime.display_mode);
            if (err != ESP_OK) {
                portENTER_CRITICAL(&s_mux);
                increment_u32(&s_stats.telemetry_publish_errors);
                portEXIT_CRITICAL(&s_mux);
            }
        }
    }
}

esp_err_t sound_meter_start(const sound_meter_config_t *config)
{
    if (s_started) {
        return ESP_OK;
    }
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_config = *config;
    if (s_config.sample_rate_hz == 0) {
        s_config.sample_rate_hz = BOARD_I2S_SAMPLE_RATE;
    }
    if (s_config.metrics_window_ms == 0) {
        s_config.metrics_window_ms = AUDIO_METRICS_DEFAULT_WINDOW_MS;
    }
    if (s_config.i2s_read_timeout_ms == 0) {
        s_config.i2s_read_timeout_ms = SOUND_METER_DEFAULT_I2S_TIMEOUT_MS;
    }
    if (s_config.pcm_chunk_bytes == 0 || s_config.pcm_chunk_bytes > BOARD_PCM_CHUNK_SIZE) {
        s_config.pcm_chunk_bytes = BOARD_PCM_CHUNK_SIZE;
    }
    s_enabled = true;
    BaseType_t ok = xTaskCreate(sound_meter_task, "sound_meter", SOUND_METER_TASK_STACK, NULL,
                                SOUND_METER_TASK_PRIORITY, &s_task);
    if (ok != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    s_started = true;
    ESP_LOGI(TAG, "sound meter started: %lu Hz %lu ms windows",
             (unsigned long)s_config.sample_rate_hz, (unsigned long)s_config.metrics_window_ms);
    return ESP_OK;
}

bool sound_meter_get_latest(audio_level_metrics_t *out)
{
    if (out == NULL) {
        return false;
    }
    portENTER_CRITICAL(&s_mux);
    bool valid = s_latest_valid;
    *out = s_latest;
    portEXIT_CRITICAL(&s_mux);
    return valid;
}

void sound_meter_get_stats(sound_meter_stats_t *out)
{
    if (out == NULL) {
        return;
    }
    portENTER_CRITICAL(&s_mux);
    *out = s_stats;
    portEXIT_CRITICAL(&s_mux);
}

void sound_meter_set_enabled(bool enabled)
{
    s_enabled = enabled;
    ESP_LOGI(TAG, "sound meter: %s", enabled ? "enabled" : "paused");
}

bool sound_meter_get_enabled(void)
{
    return s_enabled;
}
