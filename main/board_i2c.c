#include "board_i2c.h"

#include "board_sticks3.h"

#include <stdbool.h>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "BOARD_I2C";
static bool s_board_i2c_installed;
static i2c_master_bus_handle_t s_board_i2c_bus;

esp_err_t board_i2c_init(void)
{
    if (s_board_i2c_installed) {
        return ESP_OK;
    }

    i2c_master_bus_config_t i2c_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = BOARD_I2C_PORT,
        .sda_io_num = BOARD_I2C_SDA_IO,
        .scl_io_num = BOARD_I2C_SCL_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&i2c_cfg, &s_board_i2c_bus);
    if (err == ESP_OK) {
        s_board_i2c_installed = true;
        ESP_LOGI(TAG, "shared I2C master bus ready on SDA GPIO%d SCL GPIO%d at %d Hz",
                 BOARD_I2C_SDA_IO, BOARD_I2C_SCL_IO, BOARD_I2C_CLK_HZ);
    } else if (err == ESP_ERR_INVALID_STATE) {
        err = i2c_master_get_bus_handle(BOARD_I2C_PORT, &s_board_i2c_bus);
        if (err == ESP_OK) {
            s_board_i2c_installed = true;
            ESP_LOGI(TAG, "shared I2C master bus reused on SDA GPIO%d SCL GPIO%d",
                     BOARD_I2C_SDA_IO, BOARD_I2C_SCL_IO);
        }
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C master bus init failed: %s", esp_err_to_name(err));
    }
    return err;
}
