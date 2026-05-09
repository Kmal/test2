/*
 * Legacy Classic Bluetooth HFP transport from the pre-compatibility-cleanup
 * firmware. This module is intentionally not compiled for M5Stack StickS3 /
 * ESP32-S3 because ESP32-S3 does not support Bluetooth Classic / BR/EDR.
 */

#include "sdkconfig.h"

#if CONFIG_APP_TRANSPORT_HFP_LEGACY
#ifdef CONFIG_IDF_TARGET_ESP32S3
#error "ESP32-S3 does not support Bluetooth Classic / BR/EDR; legacy HFP is not available on StickS3"
#endif
#error "Legacy HFP source is quarantined and requires a non-StickS3 API refresh before it can be built"

/*
 * Main firmware entry point for the M5Stack Stick S3 wireless microphone.
 * The firmware exposes a Bluetooth Hands-Free Profile client and feeds
 * captured ES8311 microphone samples to the HFP outgoing data callback.
 * Incoming HFP audio is optionally written back to the codec DAC for
 * monitoring.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_hf_client_api.h"
#include "audio_resample.h"
#include "es8311.h"
#include "status_ui.h"
#include "board_sticks3.h"

// Tag used for log messages
static const char *TAG = "BT_MIC";

#define BT_PEER_NVS_NAMESPACE "bt_peer"
#define BT_PEER_NVS_KEY       "addr"
#define RECONNECT_BACKOFF_MS   3000
#define HFP_I2S_READ_TIMEOUT_MS 20
#define HFP_I2S_WRITE_TIMEOUT_MS 20

// Buffer size used when reading from the codec.  A small buffer
// reduces latency at the expense of CPU load.  HFP audio uses 8 kHz
// sampling rate, 16-bit mono.  Captured audio is low-pass filtered
// before 2:1 decimation to reduce aliasing harshness.
#define HFP_LOG_THROTTLE_US          (1000000LL)
#define HFP_UNDERFILL_LOG_THRESHOLD  3

/*
 * ESP-IDF HFP outgoing callbacks normally expect the callback to return the
 * requested byte count after the application has filled the whole buffer.
 * Keep returning len after zero-padding underfills unless a specific ESP-IDF
 * version/application integration defines this compatibility switch.
 */
#ifndef HFP_OUTGOING_DATA_CB_RETURN_SHORT_ON_UNDERFILL
#define HFP_OUTGOING_DATA_CB_RETURN_SHORT_ON_UNDERFILL 0
#endif

typedef enum {
    HFP_AUDIO_MODE_NONE = 0,
    HFP_AUDIO_MODE_CVSD_NARROW_BAND,
    HFP_AUDIO_MODE_MSBC_WIDE_BAND,
} hfp_audio_mode_t;

typedef struct {
    audio_resample_decimator_t outgoing_decimator;
    audio_resample_expander_t incoming_expander;
    int64_t last_read_error_log_us;
    int64_t last_write_error_log_us;
    int64_t last_underfill_log_us;
    uint32_t consecutive_underfills;
} hfp_audio_cb_state_t;

static portMUX_TYPE s_shared_state_mux = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE s_hfp_audio_mux = portMUX_INITIALIZER_UNLOCKED;

static esp_bd_addr_t peer_addr = {0};
static bool bt_ready = false;
static bool hfp_connected = false;
static bool audio_connected = false;
static bool audio_connect_pending = false;
/*
 * Set while the user-requested clear-pairing flow is in progress.  This lets
 * the button handler suppress automatic reconnect attempts without pretending
 * HFP/audio links have disconnected before ESP-IDF reports those events.
 */
static bool clear_pairing_requested = false;
static hfp_audio_mode_t hfp_audio_mode = HFP_AUDIO_MODE_NONE;
static hfp_audio_cb_state_t hfp_audio_cb_state = {
    .last_read_error_log_us = -HFP_LOG_THROTTLE_US,
    .last_write_error_log_us = -HFP_LOG_THROTTLE_US,
    .last_underfill_log_us = -HFP_LOG_THROTTLE_US,
};

/* Forward declarations */
static int hfp_outgoing_data_cb(uint8_t *data, uint32_t len);
static void hfp_incoming_data_cb(const uint8_t *data, uint32_t len);
static void hfp_event_handler(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param);
static void clear_pairing_button_cb(void *ctx);
static void toggle_monitoring_button_cb(void *ctx);
static void toggle_discoverable_button_cb(void *ctx);
static esp_err_t set_discoverable_mode(bool enabled);
static void reconnect_task(void *arg);

static void log_bd_addr(const char *prefix, const esp_bd_addr_t addr)
{
    ESP_LOGI(TAG, "%s %02x:%02x:%02x:%02x:%02x:%02x", prefix,
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

static bool peer_addr_is_empty(const esp_bd_addr_t addr)
{
    const esp_bd_addr_t empty = {0};
    return memcmp(addr, empty, sizeof(esp_bd_addr_t)) == 0;
}

static void shared_state_set_peer_addr(const esp_bd_addr_t addr)
{
    portENTER_CRITICAL(&s_shared_state_mux);
    memcpy(peer_addr, addr, sizeof(esp_bd_addr_t));
    portEXIT_CRITICAL(&s_shared_state_mux);
}

static void shared_state_clear_peer_addr(void)
{
    portENTER_CRITICAL(&s_shared_state_mux);
    memset(peer_addr, 0, sizeof(peer_addr));
    portEXIT_CRITICAL(&s_shared_state_mux);
}

static void shared_state_set_bt_ready(bool ready)
{
    portENTER_CRITICAL(&s_shared_state_mux);
    bt_ready = ready;
    portEXIT_CRITICAL(&s_shared_state_mux);
}

static bool shared_state_get_bt_ready(void)
{
    bool ready;
    portENTER_CRITICAL(&s_shared_state_mux);
    ready = bt_ready;
    portEXIT_CRITICAL(&s_shared_state_mux);
    return ready;
}

static hfp_audio_mode_t shared_state_get_audio_mode(void)
{
    hfp_audio_mode_t mode;
    portENTER_CRITICAL(&s_shared_state_mux);
    mode = hfp_audio_mode;
    portEXIT_CRITICAL(&s_shared_state_mux);
    return mode;
}

static bool shared_state_snapshot_for_audio_connect(esp_bd_addr_t addr)
{
    bool should_connect;
    portENTER_CRITICAL(&s_shared_state_mux);
    memcpy(addr, peer_addr, sizeof(esp_bd_addr_t));
    should_connect = !peer_addr_is_empty(addr) && hfp_connected &&
                     !audio_connected && !audio_connect_pending &&
                     !clear_pairing_requested;
    if (should_connect) {
        audio_connect_pending = true;
    }
    portEXIT_CRITICAL(&s_shared_state_mux);
    return should_connect;
}

static void shared_state_clear_audio_connect_pending(void)
{
    portENTER_CRITICAL(&s_shared_state_mux);
    audio_connect_pending = false;
    portEXIT_CRITICAL(&s_shared_state_mux);
}

static void hfp_audio_cb_state_reset(void)
{
    portENTER_CRITICAL(&s_hfp_audio_mux);
    audio_resample_decimator_reset(&hfp_audio_cb_state.outgoing_decimator);
    audio_resample_expander_reset(&hfp_audio_cb_state.incoming_expander);
    hfp_audio_cb_state.last_read_error_log_us = -HFP_LOG_THROTTLE_US;
    hfp_audio_cb_state.last_write_error_log_us = -HFP_LOG_THROTTLE_US;
    hfp_audio_cb_state.last_underfill_log_us = -HFP_LOG_THROTTLE_US;
    hfp_audio_cb_state.consecutive_underfills = 0;
    portEXIT_CRITICAL(&s_hfp_audio_mux);
}

static const char *hfp_audio_mode_name(hfp_audio_mode_t mode)
{
    switch (mode) {
    case HFP_AUDIO_MODE_CVSD_NARROW_BAND:
        return "CVSD narrow-band 8 kHz";
    case HFP_AUDIO_MODE_MSBC_WIDE_BAND:
        return "mSBC wide-band 16 kHz";
    case HFP_AUDIO_MODE_NONE:
    default:
        return "none";
    }
}

static esp_err_t peer_store_save(const esp_bd_addr_t addr)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(BT_PEER_NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open peer store for write: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(nvs, BT_PEER_NVS_KEY, addr, sizeof(esp_bd_addr_t));
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);

    if (err == ESP_OK) {
        log_bd_addr("Saved Bluetooth peer", addr);
    } else {
        ESP_LOGE(TAG, "Failed to save Bluetooth peer: %s", esp_err_to_name(err));
    }
    return err;
}

static esp_err_t peer_store_load(esp_bd_addr_t addr)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(BT_PEER_NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        return err;
    }

    size_t len = sizeof(esp_bd_addr_t);
    err = nvs_get_blob(nvs, BT_PEER_NVS_KEY, addr, &len);
    nvs_close(nvs);

    if (err == ESP_OK && len != sizeof(esp_bd_addr_t)) {
        ESP_LOGW(TAG, "Ignoring saved peer with invalid length: %u", (unsigned)len);
        memset(addr, 0, sizeof(esp_bd_addr_t));
        return ESP_ERR_INVALID_SIZE;
    }
    if (err == ESP_OK && peer_addr_is_empty(addr)) {
        ESP_LOGW(TAG, "Ignoring empty saved peer address");
        return ESP_ERR_INVALID_STATE;
    }
    return err;
}

static esp_err_t peer_store_clear(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(BT_PEER_NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        shared_state_clear_peer_addr();
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open peer store for clear: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_erase_key(nvs, BT_PEER_NVS_KEY);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = ESP_OK;
    }
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);

    if (err == ESP_OK) {
        shared_state_clear_peer_addr();
        ESP_LOGI(TAG, "Cleared saved Bluetooth peer");
    } else {
        ESP_LOGE(TAG, "Failed to clear Bluetooth peer: %s", esp_err_to_name(err));
    }
    return err;
}

static bool peer_reset_button_pressed(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << BOARD_BUTTON_CLEAR_PAIRING_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
    vTaskDelay(pdMS_TO_TICKS(20));
    return gpio_get_level(BOARD_BUTTON_CLEAR_PAIRING_GPIO) == BOARD_BUTTON_ACTIVE_LEVEL;
}

/* Initialise the I2S peripheral for full-duplex operation with the
 * ES8311 codec.  The ESP32-S3 is the I2S master in this firmware: it
 * drives MCLK, BCLK, and LRCLK/WS, and the ES8311 is configured as an
 * I2S slave by the codec init helper.  fixed_mclk is set to 12.288 MHz so the
 * ES8311 clock-divider register used for 16 kHz audio sees the MCLK
 * frequency it expects; APLL is enabled for stable MCLK output.
 * mclk_multiple is left at 256 for IDF bookkeeping, but fixed_mclk is
 * the board-level clock source selection.
 */
static void i2s_init(void)
{
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX,
        .sample_rate = BOARD_I2S_SAMPLE_RATE,
        .bits_per_sample = BOARD_I2S_BITS,
        .channel_format = BOARD_I2S_CHANNEL_FMT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 128,
        .use_apll = true,
        .tx_desc_auto_clear = true,
        .fixed_mclk = BOARD_I2S_MCLK_HZ,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = BOARD_I2S_BCK_IO,
        .ws_io_num = BOARD_I2S_WS_IO,
        .data_out_num = BOARD_I2S_DO_IO,
        .data_in_num = BOARD_I2S_DI_IO,
        .mck_io_num = BOARD_I2S_MCLK_IO,
    };

    ESP_ERROR_CHECK(i2s_driver_install(BOARD_I2S_PORT, &i2s_config, 0, NULL));
    ESP_ERROR_CHECK(i2s_set_pin(BOARD_I2S_PORT, &pin_config));
    // ESP32-S3 I2S master drives MCLK/BCLK/LRCLK for the ES8311 slave.
    ESP_LOGI(TAG, "I2S initialised");
}

/* Initialise the I2C bus and configure the ES8311 codec into
 * simultaneous record/playback mode.
 */
static void codec_init(void)
{
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = BOARD_I2C_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = BOARD_I2C_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    ESP_ERROR_CHECK(i2c_param_config(BOARD_I2C_PORT, &i2c_cfg));
    ESP_ERROR_CHECK(i2c_driver_install(BOARD_I2C_PORT, i2c_cfg.mode, 0, 0, 0));

    // Configure the ES8311 codec using the local minimal driver.
    ESP_ERROR_CHECK(es8311_init(BOARD_I2C_PORT, BOARD_ES8311_ADDR, BOARD_I2S_PORT, BOARD_I2S_SAMPLE_RATE));
    ESP_LOGI(TAG, "Codec initialised");
}


static esp_err_t set_discoverable_mode(bool enabled)
{
    if (!shared_state_get_bt_ready()) {
        ESP_LOGW(TAG, "Bluetooth is not ready; discoverable change ignored");
        return ESP_ERR_INVALID_STATE;
    }

    esp_bt_scan_mode_t scan_mode = enabled ? ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE
                                           : ESP_BT_SCAN_MODE_CONNECTABLE;
    esp_err_t err = esp_bt_gap_set_scan_mode(scan_mode);
    if (err == ESP_OK) {
        bool connected;
        portENTER_CRITICAL(&s_shared_state_mux);
        connected = hfp_connected || audio_connected;
        portEXIT_CRITICAL(&s_shared_state_mux);
        status_ui_set_discoverable_enabled(enabled);
        if (enabled && !connected) {
            status_ui_set_state(STATUS_UI_STATE_DISCOVERABLE);
        }
    } else {
        ESP_LOGE(TAG, "failed to update discoverable mode: %s", esp_err_to_name(err));
        status_ui_set_state(STATUS_UI_STATE_ERROR);
    }
    return err;
}

static void clear_pairing_button_cb(void *ctx)
{
    (void)ctx;
    if (!shared_state_get_bt_ready()) {
        ESP_LOGW(TAG, "Bluetooth is not ready; clear pairing ignored");
        return;
    }

    esp_bd_addr_t addr;
    bool has_peer;
    bool disconnect_audio;
    bool disconnect_hfp;
    portENTER_CRITICAL(&s_shared_state_mux);
    memcpy(addr, peer_addr, sizeof(addr));
    has_peer = !peer_addr_is_empty(addr);
    disconnect_audio = audio_connected || audio_connect_pending;
    disconnect_hfp = hfp_connected || has_peer;
    clear_pairing_requested = true;
    audio_connect_pending = false;
    portEXIT_CRITICAL(&s_shared_state_mux);

    if (disconnect_audio) {
        esp_err_t err = esp_hf_client_disconnect_audio(addr);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Audio disconnect request during clear pairing failed: %s", esp_err_to_name(err));
        }
    }
    if (disconnect_hfp && has_peer) {
        esp_err_t err = esp_hf_client_disconnect(addr);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "HFP disconnect request during clear pairing failed: %s", esp_err_to_name(err));
        }
    }

    esp_err_t err = peer_store_clear();
    if (err != ESP_OK) {
        status_ui_set_state(STATUS_UI_STATE_ERROR);
        return;
    }

    int bond_count = esp_bt_gap_get_bond_device_num();
    if (bond_count <= 0) {
        ESP_LOGI(TAG, "No bonded devices to remove");
    } else {
        esp_bd_addr_t *bonded_devices = calloc(bond_count, sizeof(esp_bd_addr_t));
        if (bonded_devices == NULL) {
            ESP_LOGE(TAG, "Unable to allocate bonded device list");
            status_ui_set_state(STATUS_UI_STATE_ERROR);
            return;
        }
        int listed_devices = bond_count;
        err = esp_bt_gap_get_bond_device_list(&listed_devices, bonded_devices);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Unable to read bonded device list: %s", esp_err_to_name(err));
            free(bonded_devices);
            status_ui_set_state(STATUS_UI_STATE_ERROR);
            return;
        }
        for (int i = 0; i < listed_devices; ++i) {
            err = esp_bt_gap_remove_bond_device(bonded_devices[i]);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Unable to remove bonded device %d: %s", i, esp_err_to_name(err));
            }
        }
        ESP_LOGI(TAG, "Removed %d bonded device(s)", listed_devices);
        free(bonded_devices);
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(set_discoverable_mode(true));
}

static void toggle_monitoring_button_cb(void *ctx)
{
    (void)ctx;
    status_ui_set_monitoring_enabled(!status_ui_get_monitoring_enabled());
}

static void toggle_discoverable_button_cb(void *ctx)
{
    (void)ctx;
    set_discoverable_mode(!status_ui_get_discoverable_enabled());
}

static void gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch(event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            log_bd_addr("Paired with", param->auth_cmpl.bd_addr);
            shared_state_set_peer_addr(param->auth_cmpl.bd_addr);
            status_ui_set_state(STATUS_UI_STATE_PAIRED);
        } else {
            ESP_LOGW(TAG, "Authentication failed");
            status_ui_set_state(STATUS_UI_STATE_ERROR);
        }
        break;
    default:
        break;
    }
}

static void maybe_save_connected_peer(const esp_bd_addr_t addr)
{
    esp_bd_addr_t saved_addr;
    if (peer_store_load(saved_addr) == ESP_OK && memcmp(saved_addr, addr, sizeof(esp_bd_addr_t)) == 0) {
        log_bd_addr("Bluetooth peer already saved", addr);
        return;
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(peer_store_save(addr));
}

static void hfp_event_handler(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param)
{
    switch(event) {
    case ESP_HF_CLIENT_CONNECTION_STATE_EVT: {
        ESP_LOGI(TAG, "HFP connection state: %d", param->conn_stat.state);
        const bool connected = param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_CONNECTED;
        const bool slc_connected = param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_SLC_CONNECTED;
        const bool disconnected = param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTED;
        if (connected || slc_connected) {
            shared_state_set_peer_addr(param->conn_stat.remote_bda);
            status_ui_set_state(STATUS_UI_STATE_PAIRED);
        }
        portENTER_CRITICAL(&s_shared_state_mux);
        hfp_connected = slc_connected;
        if (slc_connected) {
            memcpy(peer_addr, param->conn_stat.remote_bda, sizeof(esp_bd_addr_t));
            clear_pairing_requested = false;
        } else if (disconnected) {
            audio_connected = false;
            audio_connect_pending = false;
            hfp_audio_mode = HFP_AUDIO_MODE_NONE;
        }
        portEXIT_CRITICAL(&s_shared_state_mux);
        if (slc_connected) {
            maybe_save_connected_peer(param->conn_stat.remote_bda);
            status_ui_set_state(STATUS_UI_STATE_HFP_CONNECTED);
        } else if (disconnected) {
            hfp_audio_cb_state_reset();
            if (status_ui_get_discoverable_enabled()) {
                status_ui_set_state(STATUS_UI_STATE_DISCOVERABLE);
            }
        }
        break;
    }
    case ESP_HF_CLIENT_AUDIO_STATE_EVT: {
        ESP_LOGI(TAG, "HFP audio state: %d", param->audio_stat.state);
        const bool cvsd_connected = param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED;
        const bool msbc_connected = param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC;
        const bool disconnected = param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_DISCONNECTED;
        hfp_audio_mode_t mode = HFP_AUDIO_MODE_NONE;
        if (cvsd_connected) {
            mode = HFP_AUDIO_MODE_CVSD_NARROW_BAND;
        } else if (msbc_connected) {
            mode = HFP_AUDIO_MODE_MSBC_WIDE_BAND;
        }
        bool still_hfp_connected;
        portENTER_CRITICAL(&s_shared_state_mux);
        audio_connected = cvsd_connected || msbc_connected;
        if (audio_connected) {
            hfp_audio_mode = mode;
        } else if (disconnected) {
            hfp_audio_mode = HFP_AUDIO_MODE_NONE;
        }
        if (audio_connected || disconnected) {
            audio_connect_pending = false;
        }
        still_hfp_connected = hfp_connected;
        portEXIT_CRITICAL(&s_shared_state_mux);
        if (audio_connected || disconnected) {
            hfp_audio_cb_state_reset();
        }
        if (audio_connected) {
            ESP_LOGI(TAG, "HFP audio mode: %s", hfp_audio_mode_name(mode));
            status_ui_set_state(STATUS_UI_STATE_AUDIO_STREAMING);
        } else if (disconnected && still_hfp_connected) {
            status_ui_set_state(STATUS_UI_STATE_HFP_CONNECTED);
        }
        break;
    }
    default:
        break;
    }
}

/* HFP outgoing data callback.  CVSD SCO uses 8 kHz PCM, so the 16 kHz
 * microphone stream is low-pass decimated 2:1.  mSBC/WBS callbacks use 16 kHz
 * PCM and are passed through without the narrow-band decimator.
 */
static int hfp_outgoing_data_cb(uint8_t *data, uint32_t len)
{
    static int16_t input_pcm[BOARD_PCM_CHUNK_SIZE];
    static int16_t output_pcm[BOARD_PCM_CHUNK_SIZE];
    const hfp_audio_mode_t mode = shared_state_get_audio_mode();
    const size_t requested_output_samples = len / sizeof(int16_t);
    size_t output_samples_written = 0;
    size_t bytes_written = 0;
    size_t total_i2s_bytes_read = 0;
    esp_err_t last_err = ESP_OK;

    if ((len % sizeof(int16_t)) != 0) {
        ESP_LOGW(TAG, "HFP requested odd byte count: %u", (unsigned int)len);
    }
    while (output_samples_written < requested_output_samples) {
        const size_t remaining_output_samples = requested_output_samples - output_samples_written;
        size_t input_samples_needed = remaining_output_samples;
        if (mode == HFP_AUDIO_MODE_CVSD_NARROW_BAND) {
            input_samples_needed *= AUDIO_RESAMPLE_DECIMATION_FACTOR;
        }
        if (input_samples_needed > BOARD_PCM_CHUNK_SIZE) {
            input_samples_needed = BOARD_PCM_CHUNK_SIZE;
        }
        size_t bytes_read = 0;
        last_err = i2s_read(BOARD_I2S_PORT, input_pcm, input_samples_needed * sizeof(int16_t),
                            &bytes_read, pdMS_TO_TICKS(HFP_I2S_READ_TIMEOUT_MS));
        total_i2s_bytes_read += bytes_read;
        if (last_err != ESP_OK || bytes_read == 0) {
            break;
        }
        const size_t input_samples_read = bytes_read / sizeof(int16_t);
        size_t samples_to_copy;
        if (mode == HFP_AUDIO_MODE_CVSD_NARROW_BAND) {
            portENTER_CRITICAL(&s_hfp_audio_mux);
            const size_t decimated_samples = audio_resample_decimate_2to1(
                &hfp_audio_cb_state.outgoing_decimator, input_pcm, input_samples_read,
                output_pcm, sizeof(output_pcm) / sizeof(output_pcm[0]));
            portEXIT_CRITICAL(&s_hfp_audio_mux);
            samples_to_copy = decimated_samples < remaining_output_samples ? decimated_samples : remaining_output_samples;
            memcpy(data + bytes_written, output_pcm, samples_to_copy * sizeof(int16_t));
        } else {
            samples_to_copy = input_samples_read < remaining_output_samples ? input_samples_read : remaining_output_samples;
            memcpy(data + bytes_written, input_pcm, samples_to_copy * sizeof(int16_t));
        }
        output_samples_written += samples_to_copy;
        bytes_written += samples_to_copy * sizeof(int16_t);
        if (bytes_read < input_samples_needed * sizeof(int16_t)) {
            break;
        }
    }
    if (bytes_written < len) {
        const size_t zero_fill_len = len - bytes_written;
        memset(data + bytes_written, 0, zero_fill_len);
        bytes_written += zero_fill_len;
        const int64_t now_us = esp_timer_get_time();
        bool should_log = false;
        uint32_t underfills;
        portENTER_CRITICAL(&s_hfp_audio_mux);
        hfp_audio_cb_state.consecutive_underfills++;
        underfills = hfp_audio_cb_state.consecutive_underfills;
        if (underfills >= HFP_UNDERFILL_LOG_THRESHOLD &&
            now_us - hfp_audio_cb_state.last_underfill_log_us >= HFP_LOG_THROTTLE_US) {
            hfp_audio_cb_state.last_underfill_log_us = now_us;
            should_log = true;
        }
        portEXIT_CRITICAL(&s_hfp_audio_mux);
        if (should_log) {
            ESP_LOGW(TAG, "HFP outgoing audio underfill: produced %u/%u bytes, zero-filled %u bytes, i2s_read=%u bytes, consecutive=%u",
                     (unsigned int)(output_samples_written * sizeof(int16_t)), (unsigned int)len,
                     (unsigned int)zero_fill_len, (unsigned int)total_i2s_bytes_read,
                     (unsigned int)underfills);
        }
    } else {
        portENTER_CRITICAL(&s_hfp_audio_mux);
        hfp_audio_cb_state.consecutive_underfills = 0;
        portEXIT_CRITICAL(&s_hfp_audio_mux);
    }
#if HFP_OUTGOING_DATA_CB_RETURN_SHORT_ON_UNDERFILL
    return (int)(output_samples_written * sizeof(int16_t));
#else
    return (int)bytes_written;
#endif
}

static void hfp_incoming_data_cb(const uint8_t *data, uint32_t len)
{
    static int16_t expanded_pcm[BOARD_PCM_CHUNK_SIZE];
    if (!status_ui_get_monitoring_enabled()) {
        return;
    }
    const hfp_audio_mode_t mode = shared_state_get_audio_mode();
    const uint8_t *write_data = data;
    size_t write_len = len;
    if (mode == HFP_AUDIO_MODE_CVSD_NARROW_BAND) {
        const int16_t *input = (const int16_t *)data;
        const size_t input_samples = len / sizeof(int16_t);
        portENTER_CRITICAL(&s_hfp_audio_mux);
        const size_t expanded_samples = audio_resample_expand_2to1(
            &hfp_audio_cb_state.incoming_expander, input, input_samples,
            expanded_pcm, sizeof(expanded_pcm) / sizeof(expanded_pcm[0]));
        portEXIT_CRITICAL(&s_hfp_audio_mux);
        write_data = (const uint8_t *)expanded_pcm;
        write_len = expanded_samples * sizeof(int16_t);
    }
    if (write_len == 0) {
        return;
    }
    size_t bytes_written = 0;
    esp_err_t err = i2s_write(BOARD_I2S_PORT, write_data, write_len, &bytes_written,
                              pdMS_TO_TICKS(HFP_I2S_WRITE_TIMEOUT_MS));
    if (err != ESP_OK || bytes_written < write_len) {
        const int64_t now_us = esp_timer_get_time();
        bool should_log = false;
        portENTER_CRITICAL(&s_hfp_audio_mux);
        if (now_us - hfp_audio_cb_state.last_write_error_log_us >= HFP_LOG_THROTTLE_US) {
            hfp_audio_cb_state.last_write_error_log_us = now_us;
            should_log = true;
        }
        portEXIT_CRITICAL(&s_hfp_audio_mux);
        if (should_log) {
            ESP_LOGW(TAG, "I2S monitoring write underflow: err=%s, wrote=%u/%u bytes",
                     esp_err_to_name(err), (unsigned int)bytes_written, (unsigned int)write_len);
        }
    }
}

static void reconnect_task(void *arg)
{
    esp_bd_addr_t addr;
    memcpy(addr, arg, sizeof(esp_bd_addr_t));
    free(arg);
    vTaskDelay(pdMS_TO_TICKS(RECONNECT_BACKOFF_MS));
    bool already_connected;
    portENTER_CRITICAL(&s_shared_state_mux);
    already_connected = hfp_connected || clear_pairing_requested;
    portEXIT_CRITICAL(&s_shared_state_mux);
    if (!already_connected) {
        log_bd_addr("Attempting saved-peer reconnect to", addr);
        esp_err_t err = esp_hf_client_connect(addr);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Saved-peer reconnect request failed: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGI(TAG, "Skipping saved-peer reconnect; HFP is already connected or clear pairing is pending");
    }
    vTaskDelete(NULL);
}

void transport_hfp_legacy_run(void)
{
    esp_err_t ret;
    const status_ui_button_handlers_t status_handlers = {
        .clear_pairing = clear_pairing_button_cb,
        .toggle_monitoring = toggle_monitoring_button_cb,
        .toggle_discoverable = toggle_discoverable_button_cb,
    };

    ESP_ERROR_CHECK(status_ui_init(&status_handlers));
    status_ui_set_state(STATUS_UI_STATE_BOOTING);

    // Initialize NVS for Bluetooth
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (peer_reset_button_pressed()) {
        ESP_LOGI(TAG, "Peer reset button held at boot; clearing saved Bluetooth peer");
        ESP_ERROR_CHECK(peer_store_clear());
    }

    // Initialise audio peripherals
    i2s_init();
    codec_init();

    // Release BLE memory as we only use Classic Bluetooth
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    // Configure and enable the Bluetooth controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));

    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    // Set device name and enable discoverable/connectable mode
    ESP_ERROR_CHECK(esp_bt_dev_set_device_name("M5StickS3-Mic"));
    ESP_ERROR_CHECK(esp_bt_gap_register_callback(gap_callback));
    shared_state_set_bt_ready(true);
    ESP_ERROR_CHECK(set_discoverable_mode(true));

    // Initialize HFP client
    ESP_ERROR_CHECK(esp_hf_client_register_callback(hfp_event_handler));
    ESP_ERROR_CHECK(esp_hf_client_init());
    // Register data callbacks (voice over HCI must be enabled in menuconfig)
    ESP_ERROR_CHECK(esp_hf_client_register_data_callback(hfp_incoming_data_cb, hfp_outgoing_data_cb));

    esp_bd_addr_t saved_peer_addr;
    esp_err_t peer_load_err = peer_store_load(saved_peer_addr);
    if (peer_load_err == ESP_OK) {
        shared_state_set_peer_addr(saved_peer_addr);
        log_bd_addr("Startup reconnect mode; saved peer is", saved_peer_addr);
        esp_bd_addr_t *reconnect_addr = malloc(sizeof(esp_bd_addr_t));
        if (reconnect_addr == NULL) {
            ESP_LOGE(TAG, "Unable to allocate reconnect peer address");
        } else {
            memcpy(reconnect_addr, saved_peer_addr, sizeof(esp_bd_addr_t));
            BaseType_t created = xTaskCreate(reconnect_task, "bt_reconnect", 3072,
                                             reconnect_addr, 5, NULL);
            if (created != pdPASS) {
                ESP_LOGE(TAG, "Unable to start saved-peer reconnect task");
                free(reconnect_addr);
            }
        }
    } else {
        shared_state_clear_peer_addr();
        ESP_LOGI(TAG, "Startup first-pairing mode; no saved Bluetooth peer (%s)",
                 esp_err_to_name(peer_load_err));
    }

    ESP_LOGI(TAG, "Bluetooth Hands-Free client initialised");

    // The event loop runs in the background.
    while (true) {
        // If a service level connection exists and audio is not yet
        // connected, request to open the audio channel.  Once open
        // the audio callbacks will be invoked.
        esp_bd_addr_t connect_addr;
        if (shared_state_snapshot_for_audio_connect(connect_addr)) {
            ret = esp_hf_client_connect_audio(connect_addr);
            ESP_LOGI(TAG, "esp_hf_client_connect_audio returned %s (0x%x)", esp_err_to_name(ret), ret);
            if (ret != ESP_OK) {
                shared_state_clear_audio_connect_pending();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#endif /* CONFIG_APP_TRANSPORT_HFP_LEGACY */
