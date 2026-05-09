#include "ble_sound_level_protocol.h"

#include <stddef.h>
#include <stdio.h>

_Static_assert(BLE_SOUND_LEVEL_MAGIC == 0x4d4c354dU, "bad level magic");
_Static_assert(BLE_SOUND_LEVEL_VERSION == 1U, "bad level version");
_Static_assert(BLE_SOUND_STATUS_MAGIC == 0x5354354dU, "bad status magic");
_Static_assert(BLE_SOUND_STATUS_VERSION == 1U, "bad status version");
_Static_assert(sizeof(ble_sound_level_packet_t) == 44, "unexpected level packet size");
_Static_assert(offsetof(ble_sound_level_packet_t, sequence) == 8, "bad sequence offset");
_Static_assert(offsetof(ble_sound_level_packet_t, rms_dbfs_q8) == 24, "bad rms offset");
_Static_assert(offsetof(ble_sound_level_packet_t, vu_percent) == 36, "bad vu offset");
_Static_assert(sizeof(ble_sound_status_packet_t) == 36, "unexpected status packet size");
_Static_assert(offsetof(ble_sound_status_packet_t, app_mode) == 12, "bad app mode offset");
_Static_assert(offsetof(ble_sound_status_packet_t, windows_completed) == 28, "bad windows offset");

int main(void)
{
    puts("ble_sound_protocol tests passed");
    return 0;
}
