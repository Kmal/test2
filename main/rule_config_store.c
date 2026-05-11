#include "rule_config_store.h"

#include <string.h>

#ifdef ESP_PLATFORM
#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"

#define STORE_NAMESPACE "rule_auto"
#define STORE_KEY "config"
static const char *TAG = "RULE_CONFIG";
#else
static bool s_host_has_config;
static automation_config_t s_host_config;
#endif

static bool rule_config_store_migrate(automation_config_t *config)
{
    if (config == NULL) {
        return false;
    }
    if (config->schema_version == RULE_CONFIG_SCHEMA_VERSION) {
        return true;
    }
    automation_config_set_defaults(config);
    return true;
}

bool rule_config_store_open(rule_config_store_t *store)
{
    if (store == NULL) {
        return false;
    }
    memset(store, 0, sizeof(*store));
#ifdef ESP_PLATFORM
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(STORE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return false;
    }
    store->handle = (uintptr_t)handle;
#endif
    store->opened = true;
    return true;
}

bool rule_config_store_load(rule_config_store_t *store, automation_config_t *config)
{
    if (store == NULL || config == NULL || !store->opened) {
        return false;
    }
#ifdef ESP_PLATFORM
    size_t required = sizeof(*config);
    esp_err_t err = nvs_get_blob((nvs_handle_t)store->handle, STORE_KEY, config, &required);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        automation_config_set_defaults(config);
        return true;
    }
    if (err != ESP_OK || required != sizeof(*config) || !rule_config_store_migrate(config) || !automation_config_validate(config, NULL, 0)) {
        ESP_LOGW(TAG, "invalid or unreadable rule config; using safe defaults");
        automation_config_set_defaults(config);
        return true;
    }
    return true;
#else
    if (!s_host_has_config) {
        automation_config_set_defaults(config);
        return true;
    }
    *config = s_host_config;
    if (!rule_config_store_migrate(config) || !automation_config_validate(config, NULL, 0)) {
        automation_config_set_defaults(config);
    }
    return true;
#endif
}

bool rule_config_store_save(rule_config_store_t *store, const automation_config_t *config)
{
    if (store == NULL || config == NULL || !store->opened || !automation_config_validate(config, NULL, 0)) {
        return false;
    }
#ifdef ESP_PLATFORM
    esp_err_t err = nvs_set_blob((nvs_handle_t)store->handle, STORE_KEY, config, sizeof(*config));
    if (err != ESP_OK) {
        return false;
    }
    return nvs_commit((nvs_handle_t)store->handle) == ESP_OK;
#else
    s_host_config = *config;
    s_host_has_config = true;
    return true;
#endif
}

void rule_config_store_close(rule_config_store_t *store)
{
    if (store == NULL || !store->opened) {
        return;
    }
#ifdef ESP_PLATFORM
    nvs_close((nvs_handle_t)store->handle);
    store->handle = 0;
#endif
    store->opened = false;
}
