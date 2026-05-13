#pragma once

#include <stdbool.h>
#include <stddef.h>

#define APP_TIME_TIMEZONE_MAX_LEN 63u

typedef struct {
    char timezone[APP_TIME_TIMEZONE_MAX_LEN + 1u];
} app_time_config_t;

bool app_time_init(void);
bool app_time_get_config(app_time_config_t *out);
bool app_time_set_timezone(const char *timezone, bool persist);
bool app_time_format_hhmm_24h(char out[6]);
bool app_time_config_json(char *out, size_t out_len);
