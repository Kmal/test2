/*
 * Main firmware entry point for M5Stack StickS3 board bring-up.
 *
 * The previous application attempted to expose a Classic Bluetooth HFP
 * microphone. That transport is incompatible with StickS3 because ESP32-S3
 * does not support Bluetooth Classic / BR/EDR. Until a StickS3-compatible
 * transport is selected, this firmware initializes the documented board audio
 * and status peripherals and reports that no audio transport is active.
 */

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/i2s.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "board_sticks3.h"
#include "es8311.h"
#include "status_ui.h"
#include "sdkconfig.h"

#if CONFIG_APP_TRANSPORT_HFP_LEGACY && defined(CONFIG_IDF_TARGET_ESP32S3)
#error "ESP32-S3 does not support Bluetooth Classic / BR/EDR; legacy HFP is not available on StickS3"
#endif

#if CONFIG_APP_TRANSPORT_HFP_LEGACY
#include "transport_hfp_legacy.h"
#endif

static const char *TAG = "STICKS3_APP";

static void key1_pressed_cb(void *ctx)
{
    (void)ctx;
    ESP_LOGI(TAG, "KEY1 pressed; no transport action is assigned until a StickS3-compatible transport is selected");
}

static void key2_pressed_cb(void *ctx)
{
    (void)ctx;
    ESP_LOGI(TAG, "KEY2 pressed; no transport action is assigned until a StickS3-compatible transport is selected");
}

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
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
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
    ESP_LOGI(TAG, "I2S initialised for StickS3 ES8311 pins");
}

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
    ESP_ERROR_CHECK(es8311_init(BOARD_I2C_PORT, BOARD_ES8311_ADDR, BOARD_I2S_PORT, BOARD_I2S_SAMPLE_RATE));
    ESP_LOGI(TAG, "ES8311 codec initialised; BMI270=0x%02x and M5PM1=0x%02x are documented but unused",
             BOARD_BMI270_ADDR, BOARD_M5PM1_ADDR);
}

void app_main(void)
{
#if CONFIG_APP_TRANSPORT_HFP_LEGACY
    transport_hfp_legacy_run();
#else
    const status_ui_button_handlers_t status_handlers = {
        .key1_pressed = key1_pressed_cb,
        .key2_pressed = key2_pressed_cb,
    };

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(status_ui_init(&status_handlers));
    status_ui_set_state(STATUS_UI_STATE_BOOTING);

    i2s_init();
    codec_init();

    status_ui_set_monitoring_enabled(false);
    status_ui_set_service_enabled(false);
    status_ui_set_state(STATUS_UI_STATE_NO_TRANSPORT);
    ESP_LOGW(TAG, "No StickS3-compatible audio transport is selected; see docs/transport-feasibility.md");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif
}
