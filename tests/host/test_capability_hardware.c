#include "capability_registry.h"
#include "sdkconfig.h"

#include <assert.h>
#include <string.h>

int main(void)
{
    assert(capability_source_schema_supported(RULE_SOURCE_BATTERY_PERCENT));
    assert(capability_source_schema_supported(RULE_SOURCE_POWER_USB_PRESENT));
    assert(capability_source_schema_supported(RULE_SOURCE_BMI270_MOTION));
    assert(capability_source_schema_supported(RULE_SOURCE_ADC_VOLTAGE_MV));

    assert(capability_source_runtime_available(RULE_SOURCE_BATTERY_PERCENT) == (bool)CONFIG_APP_BATTERY_FACTS);
    assert(capability_source_runtime_available(RULE_SOURCE_POWER_USB_PRESENT) == (bool)CONFIG_APP_USB_POWER_FACTS);
    assert(capability_source_runtime_available(RULE_SOURCE_BMI270_MOTION) == (bool)CONFIG_APP_BMI270_FACTS);
    assert(capability_source_runtime_available(RULE_SOURCE_ADC_VOLTAGE_MV) == (bool)CONFIG_APP_ADC_FACTS);

    if (CONFIG_APP_BATTERY_FACTS) {
        assert(strcmp(capability_source_availability_reason(RULE_SOURCE_BATTERY_PERCENT), "implemented") == 0);
    } else {
        assert(strcmp(capability_source_availability_reason(RULE_SOURCE_BATTERY_PERCENT), "battery_facts_disabled") == 0);
    }
    if (CONFIG_APP_USB_POWER_FACTS) {
        assert(strcmp(capability_source_availability_reason(RULE_SOURCE_POWER_USB_PRESENT), "implemented") == 0);
    } else {
        assert(strcmp(capability_source_availability_reason(RULE_SOURCE_POWER_USB_PRESENT), "usb_power_facts_disabled") == 0);
    }
    return 0;
}
