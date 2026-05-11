#include "rule_config_store.h"

#include <stdio.h>
#include <stdlib.h>

#define ASSERT_TRUE(value) do { if (!(value)) { fprintf(stderr, "%s:%d assertion failed: %s\n", __FILE__, __LINE__, #value); exit(1); } } while (0)
#define ASSERT_FALSE(value) ASSERT_TRUE(!(value))
#define ASSERT_EQ(expected, actual) do { if ((expected) != (actual)) { fprintf(stderr, "%s:%d expected %d got %d\n", __FILE__, __LINE__, (int)(expected), (int)(actual)); exit(1); } } while (0)

static void test_missing_loads_defaults_and_invalid_save_rejected(void)
{
    rule_config_store_t store;
    automation_config_t config;
    ASSERT_TRUE(rule_config_store_open(&store));
    ASSERT_TRUE(rule_config_store_load(&store, &config));
    ASSERT_EQ(RULE_CONFIG_SCHEMA_VERSION, config.schema_version);
    config.rules[0].cooldown_ms = 0;
    ASSERT_FALSE(rule_config_store_save(&store, &config));
    rule_config_store_close(&store);
}

static void test_save_reload_valid_config(void)
{
    rule_config_store_t store;
    automation_config_t config;
    ASSERT_TRUE(rule_config_store_open(&store));
    automation_config_set_defaults(&config);
    config.rules[0].enabled = true;
    ASSERT_TRUE(rule_config_store_save(&store, &config));
    automation_config_t loaded;
    ASSERT_TRUE(rule_config_store_load(&store, &loaded));
    ASSERT_EQ(1, loaded.rules[0].enabled);
    rule_config_store_close(&store);
}

int main(void)
{
    test_missing_loads_defaults_and_invalid_save_rejected();
    test_save_reload_valid_config();
    puts("rule_config_store tests passed");
    return 0;
}
