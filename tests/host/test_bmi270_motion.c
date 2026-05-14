#include "bmi270.h"
#include "board_sticks3.h"
#include "fake_register_bus.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

static void set_accel_raw(int16_t x, int16_t y, int16_t z)
{
    const uint8_t reg = 0x0c;
    const uint16_t values[3] = {(uint16_t)x, (uint16_t)y, (uint16_t)z};
    for (size_t i = 0; i < 3; ++i) {
        fake_register_bus_set_reg(BOARD_BMI270_ADDR, (uint8_t)(reg + i * 2), (uint8_t)(values[i] & 0xffu));
        fake_register_bus_set_reg(BOARD_BMI270_ADDR, (uint8_t)(reg + i * 2 + 1), (uint8_t)(values[i] >> 8));
    }
}

static bmi270_motion_config_t motion_config(void)
{
    return (bmi270_motion_config_t) {
        .motion_delta_mg = 120,
        .still_hysteresis_mg = 30,
        .min_interval_ms = 0,
    };
}

static void test_null_pointer_validation(void)
{
    bmi270_motion_config_t cfg = motion_config();
    bmi270_motion_state_t state;
    bool motion = true;

    assert(bmi270_motion_state_init(NULL, &cfg) == ESP_ERR_INVALID_ARG);
    assert(bmi270_motion_state_init(&state, NULL) == ESP_ERR_INVALID_ARG);
    assert(bmi270_motion_poll(NULL, BOARD_I2C_PORT, BOARD_BMI270_ADDR, 0, &motion) == ESP_ERR_INVALID_ARG);
    assert(bmi270_motion_poll(&state, BOARD_I2C_PORT, BOARD_BMI270_ADDR, 0, NULL) == ESP_ERR_INVALID_ARG);
    assert(bmi270_read_accel(BOARD_I2C_PORT, BOARD_BMI270_ADDR, NULL) == ESP_ERR_INVALID_ARG);
}

static void test_chip_id_mismatch_fails_init(void)
{
    fake_register_bus_reset();
    fake_register_bus_set_reg(BOARD_BMI270_ADDR, 0x00, 0x00);
    assert(bmi270_init(BOARD_I2C_PORT, BOARD_BMI270_ADDR) == ESP_ERR_INVALID_RESPONSE);
}

static void test_chip_id_match_initializes(void)
{
    fake_register_bus_reset();
    fake_register_bus_set_reg(BOARD_BMI270_ADDR, 0x00, 0x24);
    assert(bmi270_init(BOARD_I2C_PORT, BOARD_BMI270_ADDR) == ESP_OK);
    assert(fake_register_bus_has_write(BOARD_BMI270_ADDR, 0x7c, 0x02));
    assert(fake_register_bus_has_write(BOARD_BMI270_ADDR, 0x40, 0xa8));
    assert(fake_register_bus_has_write(BOARD_BMI270_ADDR, 0x41, 0x00));
    assert(fake_register_bus_has_write(BOARD_BMI270_ADDR, 0x7d, 0x04));
}

static void test_read_accel_sample_time_is_deterministic(void)
{
    fake_register_bus_reset();
    set_accel_raw(0, 0, 0);
    bmi270_accel_sample_t sample = {
        .sample_time_ms = 1234,
    };
    assert(bmi270_read_accel(BOARD_I2C_PORT, BOARD_BMI270_ADDR, &sample) == ESP_OK);
    assert(sample.sample_time_ms == 0);
}

static void test_motion_threshold_and_hysteresis(void)
{
    bmi270_motion_config_t cfg = motion_config();
    bmi270_motion_state_t state;
    bool motion = true;

    fake_register_bus_reset();
    assert(bmi270_motion_state_init(&state, &cfg) == ESP_OK);

    set_accel_raw(0, 0, 0);
    assert(bmi270_motion_poll(&state, BOARD_I2C_PORT, BOARD_BMI270_ADDR, 0, &motion) == ESP_OK);
    assert(!motion);

    set_accel_raw(1000, 0, 0);
    assert(bmi270_motion_poll(&state, BOARD_I2C_PORT, BOARD_BMI270_ADDR, 10, &motion) == ESP_OK);
    assert(!motion);

    set_accel_raw(4000, 0, 0);
    assert(bmi270_motion_poll(&state, BOARD_I2C_PORT, BOARD_BMI270_ADDR, 20, &motion) == ESP_OK);
    assert(motion);

    set_accel_raw(5500, 0, 0);
    assert(bmi270_motion_poll(&state, BOARD_I2C_PORT, BOARD_BMI270_ADDR, 30, &motion) == ESP_OK);
    assert(motion);

    set_accel_raw(5600, 0, 0);
    assert(bmi270_motion_poll(&state, BOARD_I2C_PORT, BOARD_BMI270_ADDR, 40, &motion) == ESP_OK);
    assert(!motion);
}

int main(void)
{
    test_null_pointer_validation();
    test_chip_id_mismatch_fails_init();
    test_chip_id_match_initializes();
    test_read_accel_sample_time_is_deterministic();
    test_motion_threshold_and_hysteresis();
    return 0;
}
