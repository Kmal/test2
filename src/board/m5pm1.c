#include "m5pm1.h"
#include "board_sticks3.h"

#include "register_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stddef.h>

static const char *TAG = "M5PM1";

#define M5PM1_RETRY_DELAY_MS 5
#define M5PM1_L3B_SETTLE_MS 100

static esp_err_t m5pm1_read_u8_retry(i2c_port_t port, uint8_t addr, uint8_t reg, uint8_t *value)
{
    bool retried = false;
    esp_err_t err = register_bus_read_u8(port, addr, reg, value);
    if (err == ESP_ERR_INVALID_RESPONSE) {
        /*
         * The PMIC can NACK the first access after boot or after its I2C
         * auto-sleep window. Battery/status reads happen periodically from the
         * LCD task, so they need the same local wake retry used by register
         * updates; otherwise every first VBAT read can fail and the UI falls
         * back to the unavailable "--%" indicator forever.
         */
        ESP_LOGI(TAG, "M5PM1 reg 0x%02x read got invalid response; retrying after %u ms",
                 reg, M5PM1_RETRY_DELAY_MS);
        vTaskDelay(pdMS_TO_TICKS(M5PM1_RETRY_DELAY_MS));
        retried = true;
        err = register_bus_read_u8(port, addr, reg, value);
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "M5PM1 reg 0x%02x read failed: err=%s retried=%s",
                 reg, esp_err_to_name(err), retried ? "yes" : "no");
    }
    (void)retried;
    return err;
}

static esp_err_t m5pm1_update_u8_once(i2c_port_t port, uint8_t addr, uint8_t reg,
                                      uint8_t mask, uint8_t value,
                                      uint8_t *current_out, uint8_t *next_out)
{
    uint8_t current = 0;
    esp_err_t err = register_bus_read_u8_quiet(port, addr, reg, &current);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t next = (current & (uint8_t)~mask) | (value & mask);
    err = register_bus_write_u8(port, addr, reg, next);
    if (err == ESP_OK) {
        if (current_out != NULL) {
            *current_out = current;
        }
        if (next_out != NULL) {
            *next_out = next;
        }
    }
    return err;
}

static esp_err_t m5pm1_update_u8_retry(i2c_port_t port, uint8_t addr, uint8_t reg,
                                       uint8_t mask, uint8_t value)
{
    uint8_t current = 0;
    uint8_t next = 0;
    bool retried = false;
    esp_err_t err = m5pm1_update_u8_once(port, addr, reg, mask, value, &current, &next);
    if (err == ESP_ERR_INVALID_RESPONSE) {
        /*
         * M5PM1 can NACK/return an invalid response on the first register
         * access immediately after boot or after its I2C auto-sleep window.
         * Keep the retry local to the PMIC path so other devices still surface
         * hard I2C faults immediately.
         */
        ESP_LOGI(TAG, "M5PM1 reg 0x%02x update got invalid response; retrying after %u ms",
                 reg, M5PM1_RETRY_DELAY_MS);
        vTaskDelay(pdMS_TO_TICKS(M5PM1_RETRY_DELAY_MS));
        retried = true;
        err = m5pm1_update_u8_once(port, addr, reg, mask, value, &current, &next);
    }

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "M5PM1 reg 0x%02x update ok: current=0x%02x mask=0x%02x value=0x%02x next=0x%02x retried=%s",
                 reg, current, mask, value, next, retried ? "yes" : "no");
    } else {
        ESP_LOGE(TAG, "M5PM1 reg 0x%02x update failed: mask=0x%02x value=0x%02x err=%s retried=%s",
                 reg, mask, value, esp_err_to_name(err), retried ? "yes" : "no");
    }
    (void)retried;
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


static esp_err_t m5pm1_read_u16_mv(i2c_port_t port, uint8_t addr, uint8_t reg_l, uint16_t *out_mv)
{
    if (out_mv == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t lo = 0;
    uint8_t hi = 0;
    esp_err_t err = m5pm1_read_u8_retry(port, addr, reg_l, &lo);
    if (err != ESP_OK) {
        return err;
    }
    err = m5pm1_read_u8_retry(port, addr, (uint8_t)(reg_l + 1), &hi);
    if (err != ESP_OK) {
        return err;
    }
    /* M5PM1 upstream readReg16() returns two little-endian bytes for these
     * voltage APIs. Do not mask the high byte; VIN/5V readings need values
     * above 4095 mV for USB/external-power-present thresholds. */
    *out_mv = (uint16_t)(((uint16_t)hi << 8) | lo);
    return ESP_OK;
}

esp_err_t m5pm1_read_vref_mv(i2c_port_t port, uint8_t addr, uint16_t *out_mv)
{
    return m5pm1_read_u16_mv(port, addr, M5PM1_REG_VREF_L, out_mv);
}

esp_err_t m5pm1_read_vbat_mv(i2c_port_t port, uint8_t addr, uint16_t *out_mv)
{
    return m5pm1_read_u16_mv(port, addr, M5PM1_REG_VBAT_L, out_mv);
}

esp_err_t m5pm1_read_vin_mv(i2c_port_t port, uint8_t addr, uint16_t *out_mv)
{
    return m5pm1_read_u16_mv(port, addr, M5PM1_REG_VIN_L, out_mv);
}

esp_err_t m5pm1_read_5v_inout_mv(i2c_port_t port, uint8_t addr, uint16_t *out_mv)
{
    return m5pm1_read_u16_mv(port, addr, M5PM1_REG_5VINOUT_L, out_mv);
}

esp_err_t m5pm1_read_charge_state(i2c_port_t port, uint8_t addr, bool *out_charging)
{
    (void)port;
    (void)addr;
    if (out_charging == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    /* The provided M5PM1 source and StickS3 documentation verify voltage
     * registers and GPIO mapping, but they do not expose a source-verified
     * charging-status register/bit for StickS3. Do not guess a bit layout. */
    return ESP_ERR_NOT_SUPPORTED;
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
    ESP_LOGI(TAG, "M5PM1 L3B enable sequence start: addr=0x%02x gpio=2 active_level=high out_reg=0x%02x out_mask=0x04 out_value=0x04 settle_ms=%u",
             addr, M5PM1_REG_GPIO_OUT, M5PM1_L3B_SETTLE_MS);
    /*
     * StickS3 schematic and pin map route M5PM1/PY G2 to PYG2_L3B_EN.
     * Official M5PM1 StickS3 guidance enables L3B by configuring GPIO2 as a
     * normal push-pull output and driving it high.
     */
    esp_err_t err = m5pm1_gpio_set_function(port, addr, 2, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "M5PM1 L3B enable failed while setting GPIO2 function: %s", esp_err_to_name(err));
        return err;
    }
    err = m5pm1_gpio_set_mode(port, addr, 2, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "M5PM1 L3B enable failed while setting GPIO2 output mode: %s", esp_err_to_name(err));
        return err;
    }
    err = m5pm1_gpio_set_drive(port, addr, 2, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "M5PM1 L3B enable failed while setting GPIO2 push-pull drive: %s", esp_err_to_name(err));
        return err;
    }
    err = m5pm1_gpio_set_output(port, addr, 2, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "M5PM1 L3B enable failed while driving GPIO2 high: %s", esp_err_to_name(err));
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(M5PM1_L3B_SETTLE_MS));
    ESP_LOGI(TAG, "M5PM1 L3B enable sequence complete: GPIO2 driven high; settled %u ms",
             M5PM1_L3B_SETTLE_MS);
    return ESP_OK;
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

    ESP_LOGI(TAG, "M5PM1 LCD power sequence: L3B is enabled; writing I2C_CFG=0x00");
    err = register_bus_write_u8(port, addr, M5PM1_REG_I2C_CFG, 0x00);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "M5PM1 LCD power sequence complete: I2C_CFG=0x00");
    } else {
        ESP_LOGE(TAG, "M5PM1 LCD power sequence failed while writing I2C_CFG: %s", esp_err_to_name(err));
    }
    return err;
}
