/*
 * Main firmware entry point for the M5Stack Stick S3 wireless microphone.
 * The firmware exposes a Bluetooth Hands-Free Profile client and feeds
 * captured ES8311 microphone samples to the HFP outgoing data callback.
 * Incoming HFP audio is optionally written back to the codec DAC for
 * monitoring.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "driver/i2c.h"
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

static esp_bd_addr_t peer_addr = {0};
static bool slc_connected = false;

/* Forward declarations */
static int hfp_outgoing_data_cb(uint8_t *data, uint32_t len);
static void hfp_incoming_data_cb(const uint8_t *data, uint32_t len);
static void hfp_event_handler(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param);

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
    // Generate MCLK from the I2S peripheral using the configured pin map.
    ESP_LOGI(TAG, "I2S initialised");
}

/* Initialise the I2C bus and configure the ES8311 codec into
 * simultaneous record/playback mode.
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

    // Configure the ES8311 codec using the local minimal driver.
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
            ESP_LOGI(TAG, "Paired with %02x:%02x:%02x:%02x:%02x:%02x", param->auth_cmpl.bd_addr[0], param->auth_cmpl.bd_addr[1], param->auth_cmpl.bd_addr[2], param->auth_cmpl.bd_addr[3], param->auth_cmpl.bd_addr[4], param->auth_cmpl.bd_addr[5]);
            memcpy(peer_addr, param->auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
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
        if (param->conn_stat.state == ESP_HF_CONNECTION_STATE_CONNECTED) {
            memcpy(peer_addr, param->conn_stat.remote_bda, sizeof(esp_bd_addr_t));
        }
        break;
    case ESP_HF_CLIENT_AUDIO_STATE_EVT:
        ESP_LOGI(TAG, "HFP audio state: %d", param->audio_stat.state);
        if (param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED) {
            slc_connected = true;
        }
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

void app_main(void)
{
    esp_err_t ret;

    // Initialize NVS for Bluetooth
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
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

    ESP_LOGI(TAG, "Bluetooth Hands‑Free client initialised.  Awaiting connection…");

    // The event loop runs in the background.  The main task can
    // optionally attempt an automatic connection once the remote
    // address is known.  Without this the host (Mac) must initiate
    // the connection through its Bluetooth preferences.
    while (true) {
        // If a service level connection exists and audio is not yet
        // connected, request to open the audio channel.  Once open
        // the audio callbacks will be invoked.
        if (peer_addr[0] != 0 && slc_connected) {
            // Connect the audio channel if not already done.  The
            // return value will be ESP_HF_CLIENT_SUCCESS if the
            // operation was accepted.  The state is updated via the
            // audio state event.
            esp_hf_client_connect_audio(peer_addr);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}