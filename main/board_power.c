#include "board_power.h"

#include "board_sticks3.h"
#include "m5pm1.h"

#include "sdkconfig.h"

#include <stddef.h>
#include <string.h>

static board_power_config_t s_board_power_config;
static bool s_board_power_initialized;

static board_power_config_t board_power_default_config(void)
{
    return (board_power_config_t) {
        .usb_present_mv_threshold = CONFIG_APP_POWER_USB_PRESENT_MV,
        .battery_min_valid_mv = 2500,
        .battery_max_valid_mv = 4500,
    };
}

uint8_t board_power_lipo_percent_from_mv(uint16_t mv)
{
    static const struct {
        uint16_t mv;
        uint8_t percent;
    } curve[] = {
        {4200, 100}, {4110, 90}, {4030, 80}, {3980, 70}, {3920, 60},
        {3870, 50}, {3820, 40}, {3790, 30}, {3740, 20}, {3680, 10}, {3300, 0},
    };

    if (mv >= curve[0].mv) {
        return curve[0].percent;
    }
    for (size_t i = 1; i < sizeof(curve) / sizeof(curve[0]); ++i) {
        if (mv >= curve[i].mv) {
            const uint16_t mv_hi = curve[i - 1].mv;
            const uint16_t mv_lo = curve[i].mv;
            const uint8_t pct_hi = curve[i - 1].percent;
            const uint8_t pct_lo = curve[i].percent;
            return (uint8_t)(pct_lo + ((uint32_t)(mv - mv_lo) * (pct_hi - pct_lo)) / (mv_hi - mv_lo));
        }
    }
    return 0;
}

esp_err_t board_power_init(const board_power_config_t *config)
{
    s_board_power_config = config != NULL ? *config : board_power_default_config();
    if (s_board_power_config.battery_min_valid_mv > s_board_power_config.battery_max_valid_mv) {
        return ESP_ERR_INVALID_ARG;
    }
    s_board_power_initialized = true;
    return ESP_OK;
}

esp_err_t board_power_read_status(board_power_status_t *out_status)
{
    if (out_status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_board_power_initialized) {
        esp_err_t init_err = board_power_init(NULL);
        if (init_err != ESP_OK) {
            return init_err;
        }
    }

    memset(out_status, 0, sizeof(*out_status));

    uint16_t vbat_mv = 0;
    uint16_t vin_mv = 0;
    uint16_t fivev_mv = 0;
    bool charging = false;

    esp_err_t first_err = ESP_OK;
    bool have_power_sample = false;

    esp_err_t err = m5pm1_read_vbat_mv(BOARD_I2C_PORT, BOARD_M5PM1_ADDR, &vbat_mv);
    if (err == ESP_OK) {
        have_power_sample = true;
        out_status->vbat_mv = vbat_mv;
        out_status->valid =
            vbat_mv >= s_board_power_config.battery_min_valid_mv &&
            vbat_mv <= s_board_power_config.battery_max_valid_mv;

        if (out_status->valid) {
            out_status->battery_percent = board_power_lipo_percent_from_mv(vbat_mv);
        }
    } else {
        first_err = err;
    }

    bool vin_valid = false;
    err = m5pm1_read_vin_mv(BOARD_I2C_PORT, BOARD_M5PM1_ADDR, &vin_mv);
    if (err == ESP_OK) {
        have_power_sample = true;
        vin_valid = true;
        out_status->vin_mv = vin_mv;
    } else if (first_err == ESP_OK) {
        first_err = err;
    }

    bool fivev_valid = false;
    err = m5pm1_read_5v_inout_mv(BOARD_I2C_PORT, BOARD_M5PM1_ADDR, &fivev_mv);
    if (err == ESP_OK) {
        have_power_sample = true;
        fivev_valid = true;
        out_status->fivev_mv = fivev_mv;
    } else if (first_err == ESP_OK) {
        first_err = err;
    }

    if (m5pm1_read_charge_state(BOARD_I2C_PORT, BOARD_M5PM1_ADDR, &charging) == ESP_OK) {
        out_status->charging = charging;
    }

    const bool vin_present = vin_valid && vin_mv >= s_board_power_config.usb_present_mv_threshold;
    const bool fivev_present = fivev_valid && fivev_mv >= s_board_power_config.usb_present_mv_threshold;
    out_status->usb_present = vin_present || fivev_present;
    out_status->usb_valid = out_status->usb_present || (vin_valid && fivev_valid);

    return have_power_sample ? ESP_OK : first_err;
}

bool board_power_get_battery_percent(uint8_t *out_percent)
{
    if (out_percent == NULL) {
        return false;
    }
    board_power_status_t status;
    if (board_power_read_status(&status) != ESP_OK || !status.valid) {
        return false;
    }
    *out_percent = status.battery_percent;
    return true;
}

bool board_power_get_usb_present(bool *out_present)
{
    if (out_present == NULL) {
        return false;
    }
    board_power_status_t status;
    if (board_power_read_status(&status) != ESP_OK || !status.usb_valid) {
        return false;
    }
    *out_present = status.usb_present;
    return true;
}
