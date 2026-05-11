#pragma once

#include "rule_engine.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    ACTION_HTTP_RESULT_OK = 0,
    ACTION_HTTP_RESULT_INVALID_ARG,
    ACTION_HTTP_RESULT_INVALID_CONFIG,
    ACTION_HTTP_RESULT_NOT_READY,
    ACTION_HTTP_RESULT_HTTP_ERROR,
} action_http_result_t;

void action_http_set_network_ready(bool ready);
bool action_http_network_ready(void);
bool rule_event_to_json(const rule_event_t *event, char *out, size_t out_len);
action_http_result_t action_http_post_event(const rule_action_t *action, const rule_event_t *event);
