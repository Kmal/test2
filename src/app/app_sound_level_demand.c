#include "app_sound_level_demand.h"

#include <string.h>

void app_sound_level_demand_clear(app_sound_level_demand_t *demand)
{
    if (demand == NULL) {
        return;
    }
    memset(demand, 0, sizeof(*demand));
}

void app_sound_level_demand_update_trigger(app_sound_level_demand_t *demand,
                                           const automation_config_t *config)
{
    if (demand == NULL) {
        return;
    }
    demand->trigger = automation_config_has_enabled_sound_source(config);
}

void app_sound_level_demand_set_telemetry(app_sound_level_demand_t *demand,
                                          bool active)
{
    if (demand == NULL) {
        return;
    }
    demand->telemetry = active;
}

bool app_sound_level_demand_capture_needed(const app_sound_level_demand_t *demand)
{
    return demand != NULL && (demand->trigger || demand->telemetry);
}
