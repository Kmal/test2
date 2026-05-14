#include "board_power.h"
#include "board_sticks3.h"
#include "m5pm1.h"
#include "fake_register_bus.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

static void set_reg16(uint8_t reg, uint16_t value)
{
    fake_register_bus_set_reg(BOARD_M5PM1_ADDR, reg, (uint8_t)(value & 0xffu));
    fake_register_bus_set_reg(BOARD_M5PM1_ADDR, (uint8_t)(reg + 1), (uint8_t)(value >> 8));
}

static void init_default_test_config(void)
{
    const board_power_config_t cfg = {
        .usb_present_mv_threshold = 4400,
        .battery_min_valid_mv = 2500,
        .battery_max_valid_mv = 4500,
    };
    assert(board_power_init(&cfg) == ESP_OK);
}

static void test_battery_percent_interpolation(void)
{
    assert(board_power_lipo_percent_from_mv(4200) == 100);
    assert(board_power_lipo_percent_from_mv(3300) == 0);
    assert(board_power_lipo_percent_from_mv(3870) == 50);
}

static void test_null_validation(void)
{
    assert(board_power_read_status(NULL) == ESP_ERR_INVALID_ARG);
    assert(!board_power_get_battery_percent(NULL));
    assert(!board_power_get_usb_present(NULL));
}

static void test_usb_present_from_vin_or_5v(void)
{
    init_default_test_config();

    fake_register_bus_reset();
    set_reg16(M5PM1_REG_VBAT_L, 4000);
    set_reg16(M5PM1_REG_VIN_L, 5000);
    board_power_status_t status;
    assert(board_power_read_status(&status) == ESP_OK);
    assert(status.valid);
    assert(status.usb_present);

    fake_register_bus_reset();
    set_reg16(M5PM1_REG_VBAT_L, 4000);
    set_reg16(M5PM1_REG_5VINOUT_L, 4800);
    assert(board_power_read_status(&status) == ESP_OK);
    assert(status.usb_present);
}

static void test_usb_absent_when_both_inputs_below_threshold(void)
{
    init_default_test_config();
    fake_register_bus_reset();
    set_reg16(M5PM1_REG_VBAT_L, 4000);
    set_reg16(M5PM1_REG_VIN_L, 1000);
    set_reg16(M5PM1_REG_5VINOUT_L, 2000);

    board_power_status_t status;
    assert(board_power_read_status(&status) == ESP_OK);
    assert(status.valid);
    assert(!status.usb_present);
}

static void test_invalid_battery_range_does_not_hide_usb_status(void)
{
    init_default_test_config();
    fake_register_bus_reset();
    set_reg16(M5PM1_REG_VBAT_L, 100);
    set_reg16(M5PM1_REG_VIN_L, 5000);

    board_power_status_t status;
    assert(board_power_read_status(&status) == ESP_OK);
    assert(!status.valid);
    assert(status.usb_present);

    uint8_t percent = 0;
    bool usb_present = false;
    assert(!board_power_get_battery_percent(&percent));
    assert(board_power_get_usb_present(&usb_present));
    assert(usb_present);
}

int main(void)
{
    test_battery_percent_interpolation();
    test_null_validation();
    test_usb_present_from_vin_or_5v();
    test_usb_absent_when_both_inputs_below_threshold();
    test_invalid_battery_range_does_not_hide_usb_status();
    return 0;
}
