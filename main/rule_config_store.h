#pragma once

#include "rule_types.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool opened;
#ifdef ESP_PLATFORM
    uintptr_t handle;
#endif
} rule_config_store_t;

bool rule_config_store_open(rule_config_store_t *store);
bool rule_config_store_load(rule_config_store_t *store, automation_config_t *config);
bool rule_config_store_save(rule_config_store_t *store, const automation_config_t *config);
void rule_config_store_close(rule_config_store_t *store);
