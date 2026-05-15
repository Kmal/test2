#include "board_audio_power.h"
#include "board_sticks3.h"
#include "m5pm1.h"
#include "fake_register_bus.h"

#include <assert.h>

static void test_speaker_amp_enable_uses_m5pm1_pyg3(void)
{
    fake_register_bus_reset();
    fake_register_bus_set_reg(BOARD_M5PM1_ADDR, M5PM1_REG_GPIO_FUNC0, 0xFF);
    fake_register_bus_set_reg(BOARD_M5PM1_ADDR, M5PM1_REG_GPIO_MODE, 0x00);
    fake_register_bus_set_reg(BOARD_M5PM1_ADDR, M5PM1_REG_GPIO_DRV, 0xFF);
    fake_register_bus_set_reg(BOARD_M5PM1_ADDR, M5PM1_REG_GPIO_OUT, 0x00);

    assert(board_speaker_amp_set(BOARD_I2C_PORT, true) == ESP_OK);
    assert(fake_register_bus_has_write(BOARD_M5PM1_ADDR, M5PM1_REG_GPIO_FUNC0, 0x3F));
    assert(fake_register_bus_has_write(BOARD_M5PM1_ADDR, M5PM1_REG_GPIO_MODE, 0x08));
    assert(fake_register_bus_has_write(BOARD_M5PM1_ADDR, M5PM1_REG_GPIO_DRV, 0xF7));
    assert(fake_register_bus_has_write(BOARD_M5PM1_ADDR, M5PM1_REG_GPIO_OUT, 0x08));
}

static void test_speaker_amp_disable_drives_pyg3_low(void)
{
    fake_register_bus_reset();
    fake_register_bus_set_reg(BOARD_M5PM1_ADDR, M5PM1_REG_GPIO_FUNC0, 0xFF);
    fake_register_bus_set_reg(BOARD_M5PM1_ADDR, M5PM1_REG_GPIO_MODE, 0xFF);
    fake_register_bus_set_reg(BOARD_M5PM1_ADDR, M5PM1_REG_GPIO_DRV, 0x00);
    fake_register_bus_set_reg(BOARD_M5PM1_ADDR, M5PM1_REG_GPIO_OUT, 0xFF);

    assert(board_speaker_amp_set(BOARD_I2C_PORT, false) == ESP_OK);
    assert(fake_register_bus_has_write(BOARD_M5PM1_ADDR, M5PM1_REG_GPIO_OUT, 0xF7));
}

int main(void)
{
    test_speaker_amp_enable_uses_m5pm1_pyg3();
    test_speaker_amp_disable_drives_pyg3_low();
    return 0;
}
