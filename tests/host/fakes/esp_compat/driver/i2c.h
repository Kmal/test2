#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef int i2c_port_t;
#define I2C_NUM_0 0
#define I2C_MODE_MASTER 1
#define I2C_MASTER_WRITE 0
#define I2C_MASTER_READ 1
#define I2C_MASTER_NACK 1
typedef void *i2c_cmd_handle_t;
typedef struct { int mode; int sda_io_num; int sda_pullup_en; int scl_io_num; int scl_pullup_en; struct { int clk_speed; } master; } i2c_config_t;
