#include "app_sound_level_demand.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT_TRUE(value) do { if (!(value)) { fprintf(stderr, "%s:%d assertion failed: %s\n", __FILE__, __LINE__, #value); exit(1); } } while (0)
#define ASSERT_FALSE(value) ASSERT_TRUE(!(value))

static void make_sound_rule_config(automation_config_t *config, bool enabled)
{
    automation_config_set_defaults(config);
    config->rule_count = 1;
    config->rules[0].enabled = enabled;
    config->rules[0].when.source = RULE_SOURCE_SOUND_RMS_DBFS;
}

static void test_no_demand_stops_capture(void)
{
    app_sound_level_demand_t demand;
    automation_config_t config;

    app_sound_level_demand_clear(&demand);
    make_sound_rule_config(&config, false);
    app_sound_level_demand_update_trigger(&demand, &config);
    app_sound_level_demand_set_telemetry(&demand, false);

    ASSERT_FALSE(demand.trigger);
    ASSERT_FALSE(demand.telemetry);
    ASSERT_FALSE(app_sound_level_demand_capture_needed(&demand));
}

static void test_trigger_demand_starts_capture(void)
{
    app_sound_level_demand_t demand;
    automation_config_t config;

    app_sound_level_demand_clear(&demand);
    make_sound_rule_config(&config, true);
    app_sound_level_demand_update_trigger(&demand, &config);

    ASSERT_TRUE(demand.trigger);
    ASSERT_FALSE(demand.telemetry);
    ASSERT_TRUE(app_sound_level_demand_capture_needed(&demand));
}

static void test_telemetry_demand_starts_capture(void)
{
    app_sound_level_demand_t demand;
    automation_config_t config;

    app_sound_level_demand_clear(&demand);
    make_sound_rule_config(&config, false);
    app_sound_level_demand_update_trigger(&demand, &config);
    app_sound_level_demand_set_telemetry(&demand, true);

    ASSERT_FALSE(demand.trigger);
    ASSERT_TRUE(demand.telemetry);
    ASSERT_TRUE(app_sound_level_demand_capture_needed(&demand));
}

static void test_both_demands_need_one_shared_capture(void)
{
    app_sound_level_demand_t demand;
    automation_config_t config;

    app_sound_level_demand_clear(&demand);
    make_sound_rule_config(&config, true);
    app_sound_level_demand_update_trigger(&demand, &config);
    app_sound_level_demand_set_telemetry(&demand, true);

    ASSERT_TRUE(demand.trigger);
    ASSERT_TRUE(demand.telemetry);
    ASSERT_TRUE(app_sound_level_demand_capture_needed(&demand));
}

int main(void)
{
    test_no_demand_stops_capture();
    test_trigger_demand_starts_capture();
    test_telemetry_demand_starts_capture();
    test_both_demands_need_one_shared_capture();
    puts("app_sound_level_demand tests passed");
    return 0;
}
