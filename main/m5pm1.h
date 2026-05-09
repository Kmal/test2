#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define M5PM1_REG_DEVICE_ID     0x00
#define M5PM1_REG_DEVICE_MODEL  0x01
#define M5PM1_REG_GPIO_MODE     0x10
#define M5PM1_REG_GPIO_OUT      0x11
#define M5PM1_REG_GPIO_DRV      0x13
#define M5PM1_REG_GPIO_FUNC0    0x16
#define M5PM1_REG_GPIO_FUNC1    0x17
#define M5PM1_REG_PULSE_CTRL    0x53

typedef struct {
    uint8_t device_id;
    uint8_t device_model;
} m5pm1_identity_t;

esp_err_t m5pm1_probe(i2c_port_t port, uint8_t addr, m5pm1_identity_t *identity);
esp_err_t m5pm1_gpio_set_function(i2c_port_t port, uint8_t addr, uint8_t gpio, uint8_t function);
esp_err_t m5pm1_gpio_set_mode(i2c_port_t port, uint8_t addr, uint8_t gpio, bool output);
esp_err_t m5pm1_gpio_set_output(i2c_port_t port, uint8_t addr, uint8_t gpio, bool high);
esp_err_t m5pm1_gpio_set_drive(i2c_port_t port, uint8_t addr, uint8_t gpio, bool push_pull);

#ifdef __cplusplus
}
#endif
