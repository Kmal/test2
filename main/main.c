/*
 * Main firmware entry point for the M5Stack Stick S3 wireless microphone
 * implementation.  This example uses the ESP‑IDF classic Bluetooth stack
 * to implement a Hands‑Free Profile (HFP) client.  When paired with a
 * computer (e.g. a Mac mini) the device advertises itself as a
 * Bluetooth headset and streams microphone audio from the on‑board
 * ES8311 codec over SCO.  The microphone audio is read from the
 * codec via the I2S peripheral and supplied to the HFP stack via a
 * data callback.  Incoming audio from the remote side is optionally
 * written back to the codec’s DAC for monitoring.
 *
 * Hardware assumptions:
 *  - The Stick S3 contains an ESP32‑S3 module and an ES8311 codec.
 *  - The codec is connected over I2C for configuration and I2S for
 *    audio data.  The board provides a MEMS microphone connected to
 *    the codec’s ADC and an 8 Ω speaker connected to the DAC.  The
 *    CNX Software article describes these features, noting that the
 *    ES8311 codec provides 24‑bit resolution, a built‑in microphone
 *    with 65 dB SNR, and a speaker amplifier【412301115246439†L40-L69】.
 *
 * The Bluetooth stack is configured to expose a Hands‑Free client,
 * which acts as the headset role.  When a host (the audio gateway) is
 * paired and the audio channel is opened, the HFP layer will fetch
 * audio samples via the outgoing data callback implemented below.
 *
 * Note:  This firmware is meant as a starting point.  Additional
 * features such as button handling, volume control and automatic
 * pairing can be added as required.  It is also recommended to fine
 * tune the ES8311 register settings for optimum audio performance.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_main.h"
#include "esp_hf_client_api.h"
#include "es8311.h"

// Tag used for log messages
static const char *TAG = "BT_MIC";

/* I2S and I2C pin definitions for the M5Stack Stick S3.  The
 * board’s schematic maps the ES8311 codec to the following pins:
 *   - BCLK (bit‑clock)  → GPIO47
 *   - LRCLK (word‑select) → GPIO0
 *   - DAC data (SDO) → GPIO2
 *   - ADC data (SDI) → GPIO1
 *   - MCLK → GPIO48
 *   - I2C SDA → GPIO8
 *   - I2C SCL → GPIO18
 */
#define I2S_PORT         I2S_NUM_0
#define I2S_SAMPLE_RATE  16000
#define I2S_BITS         I2S_BITS_PER_SAMPLE_16BIT
#define I2S_CHANNEL_FMT  I2S_CHANNEL_FMT_ONLY_LEFT

#define I2S_BCK_IO       47
#define I2S_WS_IO        0
#define I2S_DO_IO        2
#define I2S_DI_IO        1
#define I2S_MCLK_IO      48

#define I2C_PORT         I2C_NUM_0
#define I2C_SDA_IO       8
#define I2C_SCL_IO       18
#define ES8311_ADDR      0x18

// Buffer size used when reading from the codec.  A small buffer
// reduces latency at the expense of CPU load.  HFP audio uses 8 kHz
// sampling rate, 16 bit mono (16 kbit/s).  When capturing at
// 16 kHz the audio is downsampled by discarding every second sample.
#define PCM_CHUNK_SIZE   320

// Hold Button A low during boot to forget the saved Bluetooth host.
// On the Stick S3 this is GPIO37; override at build time if your board differs.
#ifndef PEER_RESET_BUTTON_GPIO
#define PEER_RESET_BUTTON_GPIO GPIO_NUM_37
#endif
#define PEER_RESET_BUTTON_ACTIVE_LEVEL 0
#define RECONNECT_BACKOFF_MS 3000
#define BT_PEER_NVS_NAMESPACE "bt_peer"
#define BT_PEER_NVS_KEY "addr"

static esp_bd_addr_t peer_addr = {0};
static bool peer_addr_valid = false;
static bool slc_connected = false;
static bool audio_connected = false;

/* Forward declarations */
static int hfp_outgoing_data_cb(uint8_t *data, uint32_t len);
static void hfp_incoming_data_cb(const uint8_t *data, uint32_t len);
static void hfp_event_handler(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param);
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
        memset(peer_addr, 0, sizeof(peer_addr));
        peer_addr_valid = false;
        ESP_LOGI(TAG, "Cleared saved Bluetooth peer");
    } else {
        ESP_LOGE(TAG, "Failed to clear Bluetooth peer: %s", esp_err_to_name(err));
    }
    return err;
}

static bool peer_reset_button_pressed(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << PEER_RESET_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
    vTaskDelay(pdMS_TO_TICKS(20));
    return gpio_get_level(PEER_RESET_BUTTON_GPIO) == PEER_RESET_BUTTON_ACTIVE_LEVEL;
}

/* Initialise the I2S peripheral for full‑duplex operation with the
 * ES8311 codec.  The codec will generate MCLK and BCLK when
 * operating in master mode; however the Stick S3 routes MCLK to
 * GPIO48 which is capable of output.  The selected configuration
 * matches the codec defaults (16 kHz sampling, 16 bit, mono).
 */
static void i2s_init(void)
{
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX,
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS,
        .channel_format = I2S_CHANNEL_FMT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 128,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCK_IO,
        .ws_io_num = I2S_WS_IO,
        .data_out_num = I2S_DO_IO,
        .data_in_num = I2S_DI_IO,
        .mck_io_num = I2S_MCLK_IO,
    };

    ESP_ERROR_CHECK(i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL));
    ESP_ERROR_CHECK(i2s_set_pin(I2S_PORT, &pin_config));
    // Enable built‑in ADC/DAC interface if necessary; we leave MCLK
    // generation to the I2S peripheral by enabling APLL usage
    ESP_LOGI(TAG, "I2S initialised");
}

/* Initialise the I2C bus and configure the ES8311 codec into
 * simultaneous record/playback mode.  The component registry notes
 * that the ES8311 features high performance ADC and DAC with 24‑bit
 * resolution and sampling frequencies from 8 kHz to 96 kHz【320084825420146†L32-L42】.
 */
static void codec_init(void)
{
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &i2c_cfg));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT, i2c_cfg.mode, 0, 0, 0));

    // Configure the ES8311 codec using a minimal driver.  The helper
    // function defined in es8311.c performs a soft reset, sets the
    // sample rate and enables both ADC and DAC.  For more advanced
    // control the espressif/esp_codec_dev component can be used
    // instead【705914580000106†L231-L298】.
    ESP_ERROR_CHECK(es8311_init(I2C_PORT, ES8311_ADDR, I2S_PORT, I2S_SAMPLE_RATE));
    ESP_LOGI(TAG, "Codec initialised");
}

/* Bluetooth GAP callback.  Logs pairing and discovery events.  When
 * pairing completes the remote address is stored to allow explicit
 * connections if required.
 */
static void gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch(event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT: {
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            log_bd_addr("Paired with", param->auth_cmpl.bd_addr);
            memcpy(peer_addr, param->auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
            peer_addr_valid = true;
            peer_store_save(peer_addr);
        } else {
            ESP_LOGW(TAG, "Authentication failed");
        }
        break;
    }
    default:
        break;
    }
}

/* Hands‑Free client event handler.  This callback processes HFP
 * events such as connection state and audio state changes.  When
 * audio becomes connected, the application may start feeding
 * microphone data via the outgoing data callback.
 */
static void hfp_event_handler(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param)
{
    switch(event) {
    case ESP_HF_CLIENT_CONNECTION_STATE_EVT:
        ESP_LOGI(TAG, "HFP connection state: %d", param->conn_stat.state);
        if (param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_CONNECTED ||
            param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_SLC_CONNECTED) {
            memcpy(peer_addr, param->conn_stat.remote_bda, sizeof(esp_bd_addr_t));
            peer_addr_valid = true;
            slc_connected = (param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_SLC_CONNECTED);
        } else if (param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTED) {
            slc_connected = false;
            audio_connected = false;
        }
        break;
    case ESP_HF_CLIENT_AUDIO_STATE_EVT:
        ESP_LOGI(TAG, "HFP audio state: %d", param->audio_stat.state);
        audio_connected = (param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED ||
                           param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC);
        break;
    default:
        break;
    }
}

/* HFP outgoing data callback.  The HFP stack invokes this function
 * whenever it requires audio samples to send to the remote device.
 * The buffer length provided by the stack corresponds to one SCO
 * packet.  Audio is read from the I2S bus and downsampled from
 * 16 kHz to 8 kHz by discarding every second sample.  Returns the
 * number of bytes actually written.
 */
static int hfp_outgoing_data_cb(uint8_t *data, uint32_t len)
{
    // Temporary buffer to hold 16 kHz audio.  Each sample is 16 bits
    // so we need twice as many bytes as the number requested by HFP.
    size_t bytes_read = 0;
    static int16_t pcm16[PCM_CHUNK_SIZE];
    int16_t *out16 = (int16_t *)data;

    // Read data from I2S
    esp_err_t err = i2s_read(I2S_PORT, pcm16, sizeof(pcm16), &bytes_read, portMAX_DELAY);
    if (err != ESP_OK || bytes_read == 0) {
        memset(data, 0, len);
        return len;
    }

    // Downsample by selecting every second sample.  HFP expects
    // mono audio at 8 kHz and 16 bits per sample (len bytes is multiple of 2).
    int samples = bytes_read / sizeof(int16_t);
    int out_idx = 0;
    for (int i = 0; i < samples && out_idx * sizeof(int16_t) < len; i += 2) {
        out16[out_idx++] = pcm16[i];
    }
    return out_idx * sizeof(int16_t);
}

/* HFP incoming data callback.  Called by the HFP stack when audio
 * data arrives from the remote device (e.g. when the Mac plays a
 * tone back).  For simplicity we route it directly to the codec’s
 * DAC so the user can monitor remote audio.  If monitoring is not
 * desired this function may simply return.
 */
static void hfp_incoming_data_cb(const uint8_t *data, uint32_t len)
{
    size_t bytes_written;
    i2s_write(I2S_PORT, data, len, &bytes_written, portMAX_DELAY);
    (void)bytes_written;
}

static void reconnect_task(void *arg)
{
    esp_bd_addr_t addr;
    memcpy(addr, arg, sizeof(esp_bd_addr_t));
    vTaskDelay(pdMS_TO_TICKS(RECONNECT_BACKOFF_MS));

    if (!slc_connected) {
        log_bd_addr("Attempting saved-peer reconnect to", addr);
        esp_err_t err = esp_hf_client_connect(addr);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Saved-peer reconnect request failed: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGI(TAG, "Skipping saved-peer reconnect; HFP is already connected");
    }

    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_err_t ret;

    // Initialize NVS for Bluetooth
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

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
    ESP_ERROR_CHECK(esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE));

    // Initialize HFP client
    ESP_ERROR_CHECK(esp_hf_client_register_callback(hfp_event_handler));
    ESP_ERROR_CHECK(esp_hf_client_init());
    // Register data callbacks (voice over HCI must be enabled in menuconfig)
    ESP_ERROR_CHECK(esp_hf_client_register_data_callback(hfp_incoming_data_cb, hfp_outgoing_data_cb));

    esp_err_t peer_load_err = peer_store_load(peer_addr);
    if (peer_load_err == ESP_OK) {
        peer_addr_valid = true;
        log_bd_addr("Startup reconnect mode; saved peer is", peer_addr);
        xTaskCreate(reconnect_task, "bt_reconnect", 3072, peer_addr, 5, NULL);
    } else {
        memset(peer_addr, 0, sizeof(peer_addr));
        peer_addr_valid = false;
        ESP_LOGI(TAG, "Startup first-pairing mode; no saved Bluetooth peer (%s)",
                 esp_err_to_name(peer_load_err));
    }

    ESP_LOGI(TAG, "Bluetooth Hands-Free client initialised");

    // The event loop runs in the background.
    while (true) {
        // If a service level connection exists and audio is not yet
        // connected, request to open the audio channel.  Once open
        // the audio callbacks will be invoked.
        if (peer_addr_valid && slc_connected && !audio_connected) {
            // Connect the audio channel if not already done.  The
            // return value will be ESP_HF_CLIENT_SUCCESS if the
            // operation was accepted.  The state is updated via the
            // audio state event.
            esp_hf_client_connect_audio(peer_addr);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}