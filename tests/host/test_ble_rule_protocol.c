#include "ble_rule_protocol.h"

#include <stddef.h>
#include <stdio.h>

_Static_assert(BLE_RULE_STATUS_MAGIC == 0x5354354dU, "bad status magic");
_Static_assert(BLE_RULE_STATUS_VERSION == 1U, "bad status version");
_Static_assert(BLE_RULE_EVENT_MAGIC == 0x4552354dU, "bad rule event magic");
_Static_assert(BLE_RULE_EVENT_VERSION == 1U, "bad rule event version");
_Static_assert(sizeof(ble_rule_status_packet_t) == 16, "unexpected status packet size");
_Static_assert(offsetof(ble_rule_status_packet_t, app_mode) == 12, "bad app mode offset");
_Static_assert(sizeof(ble_rule_event_packet_t) == 52, "unexpected rule event packet size");
_Static_assert(offsetof(ble_rule_event_packet_t, rule_id) == 16, "bad rule id offset");
_Static_assert(offsetof(ble_rule_event_packet_t, rule_name) == 32, "bad rule name offset");

int main(void)
{
    puts("ble_rule_protocol tests passed");
    return 0;
}
