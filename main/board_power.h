#pragma once

#include "esp_err.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool valid;
    uint16_t vbat_mv;
    uint8_t battery_percent;
    bool usb_present;
    uint16_t vin_mv;
    uint16_t fivev_mv;
    bool charging;
} board_power_status_t;

typedef struct {
    uint16_t usb_present_mv_threshold;
    uint16_t battery_min_valid_mv;
    uint16_t battery_max_valid_mv;
} board_power_config_t;

esp_err_t board_power_init(const board_power_config_t *config);
esp_err_t board_power_read_status(board_power_status_t *out_status);
bool board_power_get_battery_percent(uint8_t *out_percent);
bool board_power_get_usb_present(bool *out_present);
uint8_t board_power_lipo_percent_from_mv(uint16_t mv);
