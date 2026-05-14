#include "m5pm1.h"
#include "fake_register_bus.h"

#include <stdio.h>
#include <stdlib.h>

#define ASSERT_TRUE(expr) do { if (!(expr)) { fprintf(stderr, "%s:%d assertion failed: %s\n", __FILE__, __LINE__, #expr); exit(1); } } while (0)
#define ASSERT_EQ(expected, actual) do { if ((expected) != (actual)) { fprintf(stderr, "%s:%d expected %ld got %ld\n", __FILE__, __LINE__, (long)(expected), (long)(actual)); exit(1); } } while (0)

static void test_function_preserves_unrelated_fields(void)
{
    fake_register_bus_reset();
    fake_register_bus_set_reg(0x6e, M5PM1_REG_GPIO_FUNC0, 0xAA);
    ASSERT_EQ(ESP_OK, m5pm1_gpio_set_function(I2C_NUM_0, 0x6e, 2, 1));
    ASSERT_TRUE(fake_register_bus_has_write(0x6e, M5PM1_REG_GPIO_FUNC0, 0x9A));
}

static void test_output_preserves_unrelated_bits(void)
{
    fake_register_bus_reset();
    fake_register_bus_set_reg(0x6e, M5PM1_REG_GPIO_OUT, 0xA0);
    ASSERT_EQ(ESP_OK, m5pm1_gpio_set_output(I2C_NUM_0, 0x6e, 2, true));
    ASSERT_TRUE(fake_register_bus_has_write(0x6e, M5PM1_REG_GPIO_OUT, 0xA4));
}

static void test_invalid_gpio_rejected(void)
{
    fake_register_bus_reset();
    ASSERT_EQ(ESP_ERR_INVALID_ARG, m5pm1_gpio_set_mode(I2C_NUM_0, 0x6e, 8, true));
}

static void test_drive_push_pull_clears_open_drain_bit(void)
{
    fake_register_bus_reset();
    fake_register_bus_set_reg(0x6e, M5PM1_REG_GPIO_DRV, 0xFF);
    ASSERT_EQ(ESP_OK, m5pm1_gpio_set_drive(I2C_NUM_0, 0x6e, 2, true));
    ASSERT_TRUE(fake_register_bus_has_write(0x6e, M5PM1_REG_GPIO_DRV, 0xFB));
}

static void test_drive_open_drain_sets_open_drain_bit(void)
{
    fake_register_bus_reset();
    fake_register_bus_set_reg(0x6e, M5PM1_REG_GPIO_DRV, 0x00);
    ASSERT_EQ(ESP_OK, m5pm1_gpio_set_drive(I2C_NUM_0, 0x6e, 2, false));
    ASSERT_TRUE(fake_register_bus_has_write(0x6e, M5PM1_REG_GPIO_DRV, 0x04));
}

static void test_read_failure_aborts_write(void)
{
    fake_register_bus_reset();
    fake_register_bus_fail_next_read(ESP_FAIL);
    ASSERT_EQ(ESP_FAIL, m5pm1_gpio_set_drive(I2C_NUM_0, 0x6e, 2, true));
    ASSERT_EQ(0, fake_register_bus_op_count());
}

static void test_lcd_power_sequence_matches_source_backed_order(void)
{
    fake_register_bus_reset();
    fake_register_bus_set_reg(0x6e, M5PM1_REG_GPIO_FUNC0, 0xFF);
    fake_register_bus_set_reg(0x6e, M5PM1_REG_GPIO_MODE, 0x00);
    fake_register_bus_set_reg(0x6e, M5PM1_REG_GPIO_DRV, 0xFF);
    fake_register_bus_set_reg(0x6e, M5PM1_REG_GPIO_OUT, 0xFF);

    ASSERT_EQ(ESP_OK, m5pm1_enable_lcd_power(I2C_NUM_0, 0x6e));
    ASSERT_TRUE(fake_register_bus_has_write(0x6e, M5PM1_REG_GPIO_FUNC0, 0xCF));
    ASSERT_TRUE(fake_register_bus_has_write(0x6e, M5PM1_REG_GPIO_MODE, 0x04));
    ASSERT_TRUE(fake_register_bus_has_write(0x6e, M5PM1_REG_GPIO_DRV, 0xFB));
    ASSERT_TRUE(fake_register_bus_has_write(0x6e, M5PM1_REG_GPIO_OUT, 0xFB));
    ASSERT_TRUE(fake_register_bus_has_write(0x6e, M5PM1_REG_I2C_CFG, 0x00));
}

static void test_lcd_power_sequence_retries_first_invalid_response(void)
{
    fake_register_bus_reset();
    fake_register_bus_set_reg(0x6e, M5PM1_REG_GPIO_FUNC0, 0xFF);
    fake_register_bus_set_reg(0x6e, M5PM1_REG_GPIO_OUT, 0xFF);
    fake_register_bus_fail_next_read(ESP_ERR_INVALID_RESPONSE);

    ASSERT_EQ(ESP_OK, m5pm1_enable_lcd_power(I2C_NUM_0, 0x6e));
    ASSERT_TRUE(fake_register_bus_has_write(0x6e, M5PM1_REG_GPIO_FUNC0, 0xCF));
    ASSERT_TRUE(fake_register_bus_has_write(0x6e, M5PM1_REG_GPIO_OUT, 0xFB));
    ASSERT_TRUE(fake_register_bus_has_write(0x6e, M5PM1_REG_I2C_CFG, 0x00));
}

static void test_read_vbat_combines_low_and_high_nibble(void)
{
    fake_register_bus_reset();
    fake_register_bus_set_reg(0x6e, M5PM1_REG_VBAT_L, 0x34);
    fake_register_bus_set_reg(0x6e, M5PM1_REG_VBAT_H, 0x2e);
    uint16_t mv = 0;
    ASSERT_EQ(ESP_OK, m5pm1_read_vbat_mv(I2C_NUM_0, 0x6e, &mv));
    ASSERT_EQ(0x0e34, mv);
}

static void test_read_vbat_retries_first_invalid_response(void)
{
    fake_register_bus_reset();
    fake_register_bus_set_reg(0x6e, M5PM1_REG_VBAT_L, 0x34);
    fake_register_bus_set_reg(0x6e, M5PM1_REG_VBAT_H, 0x0e);
    fake_register_bus_fail_next_read(ESP_ERR_INVALID_RESPONSE);
    uint16_t mv = 0;

    ASSERT_EQ(ESP_OK, m5pm1_read_vbat_mv(I2C_NUM_0, 0x6e, &mv));
    ASSERT_EQ(0x0e34, mv);
}

static void test_board_battery_percent_uses_vbat_curve(void)
{
    fake_register_bus_reset();
    fake_register_bus_set_reg(0x6e, M5PM1_REG_VBAT_L, (uint8_t)(3870 & 0xff));
    fake_register_bus_set_reg(0x6e, M5PM1_REG_VBAT_H, (uint8_t)(3870 >> 8));
    uint8_t percent = 0;
    ASSERT_TRUE(board_power_get_battery_percent(&percent));
    ASSERT_EQ(50, percent);
}

int main(void)
{
    test_function_preserves_unrelated_fields();
    test_output_preserves_unrelated_bits();
    test_invalid_gpio_rejected();
    test_drive_push_pull_clears_open_drain_bit();
    test_drive_open_drain_sets_open_drain_bit();
    test_read_failure_aborts_write();
    test_lcd_power_sequence_matches_source_backed_order();
    test_lcd_power_sequence_retries_first_invalid_response();
    test_read_vbat_combines_low_and_high_nibble();
    test_read_vbat_retries_first_invalid_response();
    test_board_battery_percent_uses_vbat_curve();
    puts("m5pm1_gpio tests passed");
    return 0;
}
