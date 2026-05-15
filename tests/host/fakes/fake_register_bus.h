#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

typedef enum { FAKE_BUS_OP_READ, FAKE_BUS_OP_WRITE } fake_bus_op_type_t;
typedef struct { fake_bus_op_type_t type; uint8_t addr; uint8_t reg; uint8_t value; } fake_bus_op_t;

void fake_register_bus_reset(void);
void fake_register_bus_set_reg(uint8_t addr, uint8_t reg, uint8_t value);
void fake_register_bus_fail_next_read(esp_err_t err);
void fake_register_bus_fail_next_write(esp_err_t err);
void fake_register_bus_fail_reg_read_once(uint8_t addr, uint8_t reg, esp_err_t err);
size_t fake_register_bus_op_count(void);
const fake_bus_op_t *fake_register_bus_op(size_t index);
int fake_register_bus_has_write(uint8_t addr, uint8_t reg, uint8_t value);
