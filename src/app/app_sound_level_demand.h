#pragma once

#include "rule_types.h"

#include <stdbool.h>

typedef struct {
    bool trigger;
    bool telemetry;
} app_sound_level_demand_t;

void app_sound_level_demand_clear(app_sound_level_demand_t *demand);
void app_sound_level_demand_update_trigger(app_sound_level_demand_t *demand,
                                           const automation_config_t *config);
void app_sound_level_demand_set_telemetry(app_sound_level_demand_t *demand,
                                          bool active);
bool app_sound_level_demand_capture_needed(const app_sound_level_demand_t *demand);
