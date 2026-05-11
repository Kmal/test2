#pragma once

#include "rule_types.h"

#include <stdbool.h>

typedef struct {
    rule_source_t source;
    bool present;
} trigger_hat_t;

bool trigger_hat_probe(trigger_hat_t *hat, rule_source_t source);
