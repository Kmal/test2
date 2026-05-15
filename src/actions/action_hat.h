#pragma once

#include "rule_types.h"

#include <stdbool.h>

bool hat_operation_supported(rule_hat_operation_t operation);
bool hat_run_operation(const rule_action_t *action);
