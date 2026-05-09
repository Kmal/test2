#include "sound_meter.h"

#include "board_i2s.h"
#include "board_sticks3.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pcm_debug_ring.h"
#include "status_ui.h"

#include <string.h>

#define SOUND_METER_TASK_STACK 4096
#define SOUND_METER_TASK_PRIORITY 5
#define SOUND_METER_DEFAULT_I2S_TIMEOUT_MS 100
#define SOUND_METER_DEFAULT_TELEMETRY_INTERVAL_MS 100
#define SOUND_METER_DEFAULT_CALIBRATION_WINDOWS 30

static const char *TAG = "SOUND_METER";

static sound_meter_config_t s_config;
static TaskHandle_t s_task;
static bool s_started;
static bool s_enabled;
static audio_level_metrics_t s_latest;
static bool s_latest_valid;
static sound_meter_stats_t s_stats;
static audio_calibration_t s_calibration;
static pcm_debug_ring_t s_pcm_ring;
static int64_t s_last_telemetry_ms;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE s_pcm_ring_mux = portMUX_INITIALIZER_UNLOCKED;

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

static bool callback_bool(sound_meter_bool_cb_t cb, bool fallback)
{
    return cb == NULL ? fallback : cb(s_config.transport_ctx);
}

static void increment_u32(uint32_t *value)
{
    if (*value != UINT32_MAX) {
        ++(*value);
    }
}

static void store_latest(const audio_level_metrics_t *metrics)
{
    portENTER_CRITICAL(&s_mux);
    s_latest = *metrics;
    s_latest_valid = true;
    s_stats.windows_completed++;
    s_stats.last_sequence = metrics->sequence;
    s_stats.last_update_ms = esp_timer_get_time() / 1000;
    portENTER_CRITICAL(&s_pcm_ring_mux);
    s_stats.pcm_debug_ring_overruns = pcm_debug_ring_get_overruns(&s_pcm_ring);
    portEXIT_CRITICAL(&s_pcm_ring_mux);
    portEXIT_CRITICAL(&s_mux);
}

static void get_stats_snapshot(sound_meter_stats_t *out)
{
    portENTER_CRITICAL(&s_mux);
    *out = s_stats;
    portEXIT_CRITICAL(&s_mux);
}

static void notify_status_update(const app_runtime_state_t *runtime)
{
    if (s_config.status_update == NULL) {
        return;
    }
    sound_meter_stats_t stats;
    audio_calibration_t calibration;
    get_stats_snapshot(&stats);
    portENTER_CRITICAL(&s_mux);
    calibration = s_calibration;
    portEXIT_CRITICAL(&s_mux);
    s_config.status_update(&stats, &calibration, runtime, s_enabled, s_config.transport_ctx);
}

static void publish_ui_snapshot(const audio_level_metrics_t *metrics, const app_runtime_state_t *runtime)
{
    audio_calibration_t calibration;
    portENTER_CRITICAL(&s_mux);
    calibration = s_calibration;
    portEXIT_CRITICAL(&s_mux);

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
        .ble_connected = callback_bool(s_config.transport_connected, false),
        .ble_metrics_notify_enabled = callback_bool(s_config.metrics_notify_enabled, false),
        .ble_pcm_notify_enabled = callback_bool(s_config.pcm_notify_enabled, false),
        .calibration_active = calibration.active,
        .calibration_collected_windows = calibration.collected_windows,
        .calibration_required_windows = calibration.required_windows,
        .calibration_noise_floor_dbfs_q8 = calibration.noise_floor_dbfs_q8,
    };
    status_ui_set_sound_meter_snapshot(&snapshot);
    portENTER_CRITICAL(&s_mux);
    increment_u32(&s_stats.lcd_updates);
    portEXIT_CRITICAL(&s_mux);
}

static void begin_calibration_if_needed(const app_runtime_state_t *runtime)
{
    if (runtime->app_mode != APP_MODE_CALIBRATION) {
        return;
    }
    portENTER_CRITICAL(&s_mux);
    bool start = !s_calibration.active && !s_calibration.valid;
    portEXIT_CRITICAL(&s_mux);
    if (!start) {
        return;
    }
    portENTER_CRITICAL(&s_mux);
    audio_calibration_begin(&s_calibration, s_config.calibration_windows);
    portEXIT_CRITICAL(&s_mux);
    ESP_LOGI(TAG, "calibration started: %lu windows", (unsigned long)s_config.calibration_windows);
}

static bool handle_calibration_window(const audio_level_metrics_t *metrics, const app_runtime_state_t *runtime)
{
    if (runtime->app_mode != APP_MODE_CALIBRATION) {
        portENTER_CRITICAL(&s_mux);
        if (!s_calibration.active) {
            s_calibration.valid = false;
        }
        portEXIT_CRITICAL(&s_mux);
        return false;
    }

    portENTER_CRITICAL(&s_mux);
    audio_calibration_add_window(&s_calibration, metrics);
    bool ready = audio_calibration_ready(&s_calibration);
    if (ready) {
        audio_calibration_finalize(&s_calibration);
    }
    portEXIT_CRITICAL(&s_mux);

    if (ready) {
        audio_calibration_t cal;
        portENTER_CRITICAL(&s_mux);
        cal = s_calibration;
        portEXIT_CRITICAL(&s_mux);
        ESP_LOGI(TAG, "calibration complete: floor=%ld.%ld dBFS loud=%ld.%ld dBFS",
                 (long)(cal.noise_floor_dbfs_q8 / 256),
                 (long)((cal.noise_floor_dbfs_q8 < 0 ? -cal.noise_floor_dbfs_q8 : cal.noise_floor_dbfs_q8) % 256) * 10 / 256,
                 (long)(cal.loud_threshold_dbfs_q8 / 256),
                 (long)((cal.loud_threshold_dbfs_q8 < 0 ? -cal.loud_threshold_dbfs_q8 : cal.loud_threshold_dbfs_q8) % 256) * 10 / 256);
    }
    return ready;
}

static void sound_meter_task(void *arg)
{
    (void)arg;
    int16_t pcm_samples[BOARD_PCM_CHUNK_SIZE / sizeof(int16_t)];
    audio_metrics_accumulator_t acc;
    audio_metrics_accumulator_init(&acc, s_config.sample_rate_hz, s_config.metrics_window_ms);
    uint32_t sequence = 0;

    while (true) {
        app_runtime_state_t runtime = get_runtime_snapshot();
        begin_calibration_if_needed(&runtime);
        if (!s_enabled || runtime.app_mode == APP_MODE_PAUSED) {
            notify_status_update(&runtime);
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        size_t bytes_read = 0;
        esp_err_t err = board_i2s_read(pcm_samples, sizeof(pcm_samples), &bytes_read,
                                       pdMS_TO_TICKS(s_config.i2s_read_timeout_ms));
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "I2S read failed: %s", esp_err_to_name(err));
            portENTER_CRITICAL(&s_mux);
            increment_u32(&s_stats.i2s_read_errors);
            portEXIT_CRITICAL(&s_mux);
            notify_status_update(&runtime);
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        if (bytes_read == 0) {
            portENTER_CRITICAL(&s_mux);
            increment_u32(&s_stats.i2s_zero_reads);
            portEXIT_CRITICAL(&s_mux);
            notify_status_update(&runtime);
            continue;
        }

        size_t sample_count = bytes_read / sizeof(pcm_samples[0]);
        if (sample_count == 0) {
            continue;
        }

        if (runtime.app_mode == APP_MODE_PCM_DEBUG_STREAM && runtime.ble_pcm_debug_enabled &&
            callback_bool(s_config.pcm_debug_enabled, false)) {
            portENTER_CRITICAL(&s_pcm_ring_mux);
            pcm_debug_ring_write(&s_pcm_ring, (const uint8_t *)pcm_samples, sample_count * sizeof(pcm_samples[0]));
            portEXIT_CRITICAL(&s_pcm_ring_mux);
        }

        audio_metrics_accumulator_add_i16(&acc, pcm_samples, sample_count);
        if (!audio_metrics_accumulator_ready(&acc)) {
            continue;
        }

        audio_level_metrics_t metrics;
        audio_calibration_t calibration_snapshot;
        portENTER_CRITICAL(&s_mux);
        calibration_snapshot = s_calibration;
        portEXIT_CRITICAL(&s_mux);
        audio_metrics_config_t metrics_config = {
            .floor_dbfs_q8 = calibration_snapshot.valid ? calibration_snapshot.noise_floor_dbfs_q8 : s_config.dbfs_floor_q8,
            .ceiling_dbfs_q8 = s_config.dbfs_ceiling_q8,
            .loud_threshold_dbfs_q8 = calibration_snapshot.valid ? calibration_snapshot.loud_threshold_dbfs_q8 : s_config.loud_threshold_q8,
        };
        if (!audio_metrics_accumulator_finalize_with_config(&acc, sequence++, &metrics_config, &metrics)) {
            continue;
        }
        store_latest(&metrics);
        bool calibration_done = handle_calibration_window(&metrics, &runtime);
        if (s_config.enable_lcd_updates) {
            publish_ui_snapshot(&metrics, &runtime);
        }
        int64_t now_ms = esp_timer_get_time() / 1000;
        bool telemetry_due = (s_last_telemetry_ms == 0) ||
                             (now_ms - s_last_telemetry_ms >= (int64_t)s_config.telemetry_interval_ms);
        if (telemetry_due && s_config.enable_ble_telemetry && runtime.ble_telemetry_enabled &&
            runtime.app_mode != APP_MODE_PAUSED && s_config.publish_metrics != NULL) {
            err = s_config.publish_metrics(&metrics, runtime.app_mode, runtime.display_mode, s_config.transport_ctx);
            if (err != ESP_OK) {
                portENTER_CRITICAL(&s_mux);
                increment_u32(&s_stats.telemetry_publish_errors);
                portEXIT_CRITICAL(&s_mux);
            } else {
                s_last_telemetry_ms = now_ms;
            }
        }
        notify_status_update(&runtime);
        if (calibration_done) {
            ESP_LOGI(TAG, "calibration complete; use control or KEY2 to return to sound meter mode");
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
    if (s_config.dbfs_ceiling_q8 <= s_config.dbfs_floor_q8) {
        s_config.dbfs_floor_q8 = -60 * 256;
        s_config.dbfs_ceiling_q8 = 0;
    }
    if (s_config.telemetry_interval_ms == 0) {
        s_config.telemetry_interval_ms = SOUND_METER_DEFAULT_TELEMETRY_INTERVAL_MS;
    }
    if (s_config.calibration_windows == 0) {
        s_config.calibration_windows = SOUND_METER_DEFAULT_CALIBRATION_WINDOWS;
    }
    portENTER_CRITICAL(&s_pcm_ring_mux);
    pcm_debug_ring_init(&s_pcm_ring);
    portEXIT_CRITICAL(&s_pcm_ring_mux);
    audio_calibration_init(&s_calibration);
    s_enabled = true;
    BaseType_t ok = xTaskCreate(sound_meter_task, "sound_meter", SOUND_METER_TASK_STACK, NULL,
                                SOUND_METER_TASK_PRIORITY, &s_task);
    if (ok != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    s_started = true;
    ESP_LOGI(TAG, "sound meter started: %lu Hz %lu ms windows telemetry=%lu ms",
             (unsigned long)s_config.sample_rate_hz, (unsigned long)s_config.metrics_window_ms,
             (unsigned long)s_config.telemetry_interval_ms);
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
    get_stats_snapshot(out);
}

bool sound_meter_get_calibration(audio_calibration_t *out)
{
    if (out == NULL) {
        return false;
    }
    portENTER_CRITICAL(&s_mux);
    *out = s_calibration;
    bool valid = s_calibration.valid || s_calibration.active;
    portEXIT_CRITICAL(&s_mux);
    return valid;
}

void sound_meter_reset_calibration(void)
{
    portENTER_CRITICAL(&s_mux);
    audio_calibration_init(&s_calibration);
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

size_t sound_meter_read_pcm_debug(uint8_t *dst, size_t max_bytes)
{
    portENTER_CRITICAL(&s_pcm_ring_mux);
    size_t bytes = pcm_debug_ring_read(&s_pcm_ring, dst, max_bytes);
    portEXIT_CRITICAL(&s_pcm_ring_mux);
    return bytes;
}

uint32_t sound_meter_get_pcm_debug_overruns(void)
{
    portENTER_CRITICAL(&s_pcm_ring_mux);
    uint32_t overruns = pcm_debug_ring_get_overruns(&s_pcm_ring);
    portEXIT_CRITICAL(&s_pcm_ring_mux);
    return overruns;
}
