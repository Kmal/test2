#include "m5pm1.h"
#include "board_sticks3.h"
#include "fake_register_bus.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

static void set_reg16(uint8_t reg, uint16_t value)
{
    fake_register_bus_set_reg(BOARD_M5PM1_ADDR, reg, (uint8_t)(value & 0xffu));
    fake_register_bus_set_reg(BOARD_M5PM1_ADDR, (uint8_t)(reg + 1), (uint8_t)(value >> 8));
}

static void test_vbat_full_little_endian_decode(void)
{
    fake_register_bus_reset();
    set_reg16(M5PM1_REG_VBAT_L, 0x1234u);

    uint16_t mv = 0;
    assert(m5pm1_read_vbat_mv(BOARD_I2C_PORT, BOARD_M5PM1_ADDR, &mv) == ESP_OK);
    assert(mv == 0x1234u);
}

static void test_vin_and_5v_can_report_usb_voltage(void)
{
    fake_register_bus_reset();
    set_reg16(M5PM1_REG_VIN_L, 5000u);
    set_reg16(M5PM1_REG_5VINOUT_L, 4800u);

    uint16_t mv = 0;
    assert(m5pm1_read_vin_mv(BOARD_I2C_PORT, BOARD_M5PM1_ADDR, &mv) == ESP_OK);
    assert(mv == 5000u);
    assert(m5pm1_read_5v_inout_mv(BOARD_I2C_PORT, BOARD_M5PM1_ADDR, &mv) == ESP_OK);
    assert(mv == 4800u);
}

static void test_invalid_null_output_pointers(void)
{
    assert(m5pm1_read_vbat_mv(0, 0, NULL) == ESP_ERR_INVALID_ARG);
    assert(m5pm1_read_vin_mv(0, 0, NULL) == ESP_ERR_INVALID_ARG);
    assert(m5pm1_read_5v_inout_mv(0, 0, NULL) == ESP_ERR_INVALID_ARG);
    assert(m5pm1_read_charge_state(0, 0, NULL) == ESP_ERR_INVALID_ARG);
}

static void test_charge_state_is_not_guessed(void)
{
    bool charging = true;
    assert(m5pm1_read_charge_state(0, 0, &charging) == ESP_ERR_NOT_SUPPORTED);
}

int main(void)
{
    test_vbat_full_little_endian_decode();
    test_vin_and_5v_can_report_usb_voltage();
    test_invalid_null_output_pointers();
    test_charge_state_is_not_guessed();
    return 0;
}
