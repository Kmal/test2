#include "register_bus.h"

#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include <stdbool.h>

#define REGISTER_BUS_TIMEOUT_MS 1000

static const char *TAG = "REG_BUS";

esp_err_t register_bus_write_u8(i2c_port_t port, uint8_t dev_addr, uint8_t reg, uint8_t value)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(REGISTER_BUS_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C write dev 0x%02x reg 0x%02x failed: %s", dev_addr, reg, esp_err_to_name(err));
    }
    return err;
}

esp_err_t register_bus_read_u8(i2c_port_t port, uint8_t dev_addr, uint8_t reg, uint8_t *value)
{
    if (value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, value, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(REGISTER_BUS_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C read dev 0x%02x reg 0x%02x failed: %s", dev_addr, reg, esp_err_to_name(err));
    }
    return err;
}

esp_err_t register_bus_update_u8(i2c_port_t port, uint8_t dev_addr, uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t current = 0;
    esp_err_t err = register_bus_read_u8(port, dev_addr, reg, &current);
    if (err != ESP_OK) {
        return err;
    }
    uint8_t next = (current & (uint8_t)~mask) | (value & mask);
    return register_bus_write_u8(port, dev_addr, reg, next);
}
