#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef int i2c_port_t;
typedef void *i2c_master_bus_handle_t;
typedef void *i2c_master_dev_handle_t;
#define I2C_NUM_0 0
#define I2C_CLK_SRC_DEFAULT 0
#define I2C_ADDR_BIT_LEN_7 7
typedef struct {
    int clk_source;
    i2c_port_t i2c_port;
    int sda_io_num;
    int scl_io_num;
    int glitch_ignore_cnt;
    struct { bool enable_internal_pullup; } flags;
} i2c_master_bus_config_t;
typedef struct {
    int dev_addr_length;
    uint8_t device_address;
    int scl_speed_hz;
} i2c_device_config_t;
