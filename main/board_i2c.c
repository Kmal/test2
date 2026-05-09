#include "board_i2c.h"

#include "board_sticks3.h"

#include <stdbool.h>
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "BOARD_I2C";
static bool s_board_i2c_installed;

esp_err_t board_i2c_init(void)
{
    if (s_board_i2c_installed) {
        return ESP_OK;
    }

    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = BOARD_I2C_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = BOARD_I2C_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = BOARD_I2C_CLK_HZ,
    };
    esp_err_t err = i2c_param_config(BOARD_I2C_PORT, &i2c_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(err));
        return err;
    }
    err = i2c_driver_install(BOARD_I2C_PORT, i2c_cfg.mode, 0, 0, 0);
    if (err == ESP_OK) {
        s_board_i2c_installed = true;
        ESP_LOGI(TAG, "shared I2C bus ready on SDA GPIO%d SCL GPIO%d", BOARD_I2C_SDA_IO, BOARD_I2C_SCL_IO);
    } else {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(err));
    }
    return err;
}
