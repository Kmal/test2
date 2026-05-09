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

static void test_read_failure_aborts_write(void)
{
    fake_register_bus_reset();
    fake_register_bus_fail_next_read(ESP_FAIL);
    ASSERT_EQ(ESP_FAIL, m5pm1_gpio_set_drive(I2C_NUM_0, 0x6e, 2, true));
    ASSERT_EQ(0, fake_register_bus_op_count());
}

int main(void)
{
    test_function_preserves_unrelated_fields();
    test_output_preserves_unrelated_bits();
    test_invalid_gpio_rejected();
    test_read_failure_aborts_write();
    puts("m5pm1_gpio tests passed");
    return 0;
}
