#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int16_t x_mg;
    int16_t y_mg;
    int16_t z_mg;
    uint32_t sample_time_ms;
} bmi270_accel_sample_t;

typedef struct {
    uint16_t motion_delta_mg;
    uint16_t still_hysteresis_mg;
    uint32_t min_interval_ms;
} bmi270_motion_config_t;

typedef struct {
    bool initialized;
    bmi270_accel_sample_t last_sample;
    bool last_motion;
    bmi270_motion_config_t config;
} bmi270_motion_state_t;

esp_err_t bmi270_init(i2c_port_t port, uint8_t addr);
esp_err_t bmi270_read_accel(i2c_port_t port, uint8_t addr, bmi270_accel_sample_t *out_sample);
esp_err_t bmi270_motion_state_init(bmi270_motion_state_t *state,
                                   const bmi270_motion_config_t *config);
esp_err_t bmi270_motion_poll(bmi270_motion_state_t *state,
                             i2c_port_t port,
                             uint8_t addr,
                             uint32_t uptime_ms,
                             bool *out_motion);
