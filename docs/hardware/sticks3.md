# M5Stack StickS3 hardware facts

This document records the StickS3 hardware facts that the firmware is allowed to rely on. Keep this file, `docs/hardware/sticks3.board.json`, `main/board_sticks3.h`, and `README.md` synchronized.

## Verified facts

| Item | Value | Source |
| --- | --- | --- |
| Controller | ESP32-S3-PICO-1-N8R8 | M5Stack StickS3 documentation |
| Bluetooth capability relevant to this project | ESP32-S3 supports Bluetooth LE, not Bluetooth Classic / BR/EDR | Espressif ESP32-S3 Bluetooth documentation |
| Audio codec | ES8311 | M5Stack StickS3 documentation and schematic |
| ES8311 I2C address | `0x18` | M5Stack StickS3 pin map |
| ES8311 MCLK | GPIO18 | M5Stack StickS3 pin map |
| ES8311 DOUT / DAC data | GPIO14 | M5Stack StickS3 pin map |
| ES8311 BCLK | GPIO17 | M5Stack StickS3 pin map |
| ES8311 LRCK / WS | GPIO15 | M5Stack StickS3 pin map |
| ES8311 DIN / ADC data | GPIO16 | M5Stack StickS3 pin map |
| ES8311 I2C SCL | GPIO48 | M5Stack StickS3 pin map |
| ES8311 I2C SDA | GPIO47 | M5Stack StickS3 pin map |
| IMU | BMI270 | M5Stack StickS3 documentation |
| BMI270 I2C address | `0x68` | M5Stack StickS3 pin map |
| PMIC / helper controller | M5PM1 | M5Stack StickS3 pin map |
| M5PM1 I2C address | `0x6e` | M5Stack StickS3 pin map |
| Speaker control function | `PYG3_SPK_Pulse` through M5PM1 | M5Stack StickS3 pin map |
| IMU interrupt function | `PYG4_IMU_INT` through M5PM1 | M5Stack StickS3 pin map |
| User key 1 | GPIO11 / `KEY1` | M5Stack StickS3 pin map |
| User key 2 | GPIO12 / `KEY2` | M5Stack StickS3 pin map |
| LCD MOSI | GPIO39 | M5Stack StickS3 pin map |
| IR TX | GPIO46 | M5Stack StickS3 pin map |
| IR RX | GPIO42 | M5Stack StickS3 pin map |

## Current firmware status

The repository previously described the StickS3 firmware as a Classic Bluetooth HFP microphone. That is not a valid StickS3 implementation because the StickS3 controller is ESP32-S3, and ESP32-S3 does not support Bluetooth Classic / BR/EDR. The legacy HFP source is retained as quarantined historical code and intentionally errors if selected until refreshed for a non-StickS3 target.

## Button notes

Only GPIO11 (`KEY1`) and GPIO12 (`KEY2`) are treated as StickS3 user keys. GPIO35, GPIO37, and GPIO39 are not used as status buttons. GPIO39 is documented as LCD MOSI and must not be configured as a button.

The exact safe end-user gesture set is intentionally minimal until the physical relationship between `KEY1`, `KEY2`, and the side power/download behavior is verified. The current firmware polls the two documented keys and logs their presses; product actions that require more than two safe gestures remain transport/UX decisions.

## Audio output and M5PM1 speaker control

The StickS3 includes an AW8737 speaker amplifier and M5PM1 speaker-control function (`PYG3_SPK_Pulse`). The exact M5PM1 command/register sequence for enabling and disabling the amplifier is not implemented here because it must be verified from official M5PM1 protocol documentation or source before driver code is written.

Speaker monitoring remains disabled as a product feature until a compatible transport requires local output and the M5PM1 speaker-control protocol is implemented. The StickS3 documentation notes that IR reception requires the speaker amplifier to be off.

## BMI270 scope

BMI270 is documented on the shared I2C bus at `0x68`, with interrupt routing through M5PM1. The current firmware does not initialize or use BMI270. This is intentional: no IMU-dependent feature is currently selected.

## Unknowns / deferred decisions

- Replacement audio transport for StickS3.
- Exact M5PM1 speaker amplifier command sequence.
- Final safe button gesture UX beyond detecting `KEY1` and `KEY2` presses.
- Whether BMI270 is needed by any future product feature.

## Source references

- M5Stack StickS3 documentation and pin map: https://docs.m5stack.com/en/core/StickS3
- StickS3 schematic PDF: https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150_Stick_S3_PRJ_V0.6_20251111_2025_11_17_16_10_24.pdf
- ES8311 datasheet: https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/products/atom/Atomic%20Echo%20Base/ES8311.pdf
- BMI270 datasheet: https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/core/K128%20CoreS3/BMI270.PDF
- Espressif Bluetooth architecture documentation: https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/bt-architecture/overview.html
