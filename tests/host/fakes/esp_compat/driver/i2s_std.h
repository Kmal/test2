#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "driver/i2s_types.h"
typedef void *i2s_chan_handle_t;
#define I2S_ROLE_MASTER 1
#define I2S_GPIO_UNUSED (-1)
#define I2S_DATA_BIT_WIDTH_16BIT 16
#define I2S_SLOT_MODE_MONO 1
#define I2S_MCLK_MULTIPLE_768 768
#define I2S_CHANNEL_DEFAULT_CONFIG(port, role) ((i2s_chan_config_t){.id=(port), .role=(role)})
#define I2S_STD_CLK_DEFAULT_CONFIG(rate) ((i2s_std_clk_config_t){.sample_rate_hz=(rate)})
#define I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(width, mode) ((i2s_std_slot_config_t){.data_bit_width=(width), .slot_mode=(mode)})
typedef struct { int id; int role; } i2s_chan_config_t;
typedef struct { int sample_rate_hz; int mclk_multiple; } i2s_std_clk_config_t;
typedef struct { int data_bit_width; int slot_mode; } i2s_std_slot_config_t;
typedef struct { int mclk; int bclk; int ws; int dout; int din; struct { bool mclk_inv; bool bclk_inv; bool ws_inv; } invert_flags; } i2s_std_gpio_config_t;
typedef struct { i2s_std_clk_config_t clk_cfg; i2s_std_slot_config_t slot_cfg; i2s_std_gpio_config_t gpio_cfg; } i2s_std_config_t;
