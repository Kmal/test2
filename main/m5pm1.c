#include "m5pm1.h"

#include "register_bus.h"
#include "esp_log.h"

#include <stddef.h>

static const char *TAG = "M5PM1";

static esp_err_t m5pm1_update_bit(i2c_port_t port, uint8_t addr, uint8_t reg, uint8_t gpio, bool set)
{
    if (gpio > 7) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t mask = (uint8_t)(1U << gpio);
    return register_bus_update_u8(port, addr, reg, mask, set ? mask : 0);
}

esp_err_t m5pm1_probe(i2c_port_t port, uint8_t addr, m5pm1_identity_t *identity)
{
    if (identity == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = register_bus_read_u8(port, addr, M5PM1_REG_DEVICE_ID, &identity->device_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Device_ID read failed: %s", esp_err_to_name(err));
        return err;
    }
    err = register_bus_read_u8(port, addr, M5PM1_REG_DEVICE_MODEL, &identity->device_model);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Device_Model read failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "M5PM1 probe: id=0x%02x model=0x%02x", identity->device_id, identity->device_model);
    return ESP_OK;
}

esp_err_t m5pm1_gpio_set_function(i2c_port_t port, uint8_t addr, uint8_t gpio, uint8_t function)
{
    if (gpio > 7 || function > 3) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t reg = (gpio < 4) ? M5PM1_REG_GPIO_FUNC0 : M5PM1_REG_GPIO_FUNC1;
    uint8_t shift = (uint8_t)((gpio % 4) * 2);
    uint8_t mask = (uint8_t)(0x3U << shift);
    uint8_t value = (uint8_t)((function & 0x3U) << shift);
    return register_bus_update_u8(port, addr, reg, mask, value);
}

esp_err_t m5pm1_gpio_set_mode(i2c_port_t port, uint8_t addr, uint8_t gpio, bool output)
{
    return m5pm1_update_bit(port, addr, M5PM1_REG_GPIO_MODE, gpio, output);
}

esp_err_t m5pm1_gpio_set_output(i2c_port_t port, uint8_t addr, uint8_t gpio, bool high)
{
    return m5pm1_update_bit(port, addr, M5PM1_REG_GPIO_OUT, gpio, high);
}

esp_err_t m5pm1_gpio_set_drive(i2c_port_t port, uint8_t addr, uint8_t gpio, bool push_pull)
{
    return m5pm1_update_bit(port, addr, M5PM1_REG_GPIO_DRV, gpio, push_pull);
}
