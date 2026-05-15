#include "trigger_hat.h"
#include "capability_registry.h"

bool trigger_hat_probe(trigger_hat_t *hat, rule_source_t source)
{
    if (hat == 0) {
        return false;
    }
    hat->source = source;
    hat->present = capability_hat_supported(source);
    return hat->present;
}
