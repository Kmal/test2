#include "action_hat.h"

bool hat_operation_supported(rule_hat_operation_t operation)
{
    (void)operation;
    return false;
}

bool hat_run_operation(const rule_action_t *action)
{
    (void)action;
    return false;
}
