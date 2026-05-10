#include "m5pm1.h"

#include "register_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stddef.h>

static const char *TAG = "M5PM1";

#define M5PM1_RETRY_DELAY_MS 5

static esp_err_t m5pm1_update_u8_retry(i2c_port_t port, uint8_t addr, uint8_t reg,
                                       uint8_t mask, uint8_t value)
{
    esp_err_t err = register_bus_update_u8(port, addr, reg, mask, value);
    if (err == ESP_ERR_INVALID_RESPONSE) {
        /*
         * M5PM1 can NACK/return an invalid response on the first register
         * access immediately after boot or after its I2C auto-sleep window.
         * Keep the retry local to the PMIC path so other devices still surface
         * hard I2C faults immediately.
         */
        ESP_LOGW(TAG, "M5PM1 reg 0x%02x update got invalid response; retrying after %u ms",
                 reg, M5PM1_RETRY_DELAY_MS);
        vTaskDelay(pdMS_TO_TICKS(M5PM1_RETRY_DELAY_MS));
        err = register_bus_update_u8(port, addr, reg, mask, value);
    }
    return err;
}

static esp_err_t m5pm1_update_bit(i2c_port_t port, uint8_t addr, uint8_t reg, uint8_t gpio, bool set)
{
    if (gpio > 7) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t mask = (uint8_t)(1U << gpio);
    return m5pm1_update_u8_retry(port, addr, reg, mask, set ? mask : 0);
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
    return m5pm1_update_u8_retry(port, addr, reg, mask, value);
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
    /* M5PM1 GPIO drive bit: 0 = push-pull, 1 = open-drain. */
    return m5pm1_update_bit(port, addr, M5PM1_REG_GPIO_DRV, gpio, !push_pull);
}

static esp_err_t m5pm1_configure_l3b_enable(i2c_port_t port, uint8_t addr)
{
    /*
     * StickS3 schematic and pin map route M5PM1/PY G2 to PYG2_L3B_EN.
     * Configure GPIO2 as a normal output, push-pull drive, and low output;
     * StickS3 inverts PYG2_L3B_EN, and the official M5PM1 sequence drives
     * PYG2 low to enable L3B peripherals.
     */
    esp_err_t err = m5pm1_gpio_set_function(port, addr, 2, 0);
    if (err != ESP_OK) {
        return err;
    }
    err = m5pm1_gpio_set_mode(port, addr, 2, true);
    if (err != ESP_OK) {
        return err;
    }
    err = m5pm1_gpio_set_drive(port, addr, 2, true);
    if (err != ESP_OK) {
        return err;
    }
    return m5pm1_gpio_set_output(port, addr, 2, false);
}

esp_err_t m5pm1_enable_l3b_power(i2c_port_t port, uint8_t addr)
{
    return m5pm1_configure_l3b_enable(port, addr);
}

esp_err_t m5pm1_enable_lcd_power(i2c_port_t port, uint8_t addr)
{
    esp_err_t err = m5pm1_configure_l3b_enable(port, addr);
    if (err != ESP_OK) {
        return err;
    }

    return register_bus_write_u8(port, addr, M5PM1_REG_I2C_CFG, 0x00);
}
