# M5Stack StickS3 hardware facts

This document records the StickS3 hardware facts that the firmware is allowed to rely on. Keep this file, `docs/hardware/sticks3.board.json`, `main/board_sticks3.h`, and `README.md` synchronized.

## Evidence map

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
| Shared I2C SCL | GPIO48 | M5Stack StickS3 pin map |
| Shared I2C SDA | GPIO47 | M5Stack StickS3 pin map |
| IMU | BMI270 | M5Stack StickS3 documentation |
| BMI270 I2C address | `0x68` | M5Stack StickS3 pin map |
| PMIC / helper controller | M5PM1 | M5Stack StickS3 pin map |
| M5PM1 I2C address | `0x6e` | M5Stack StickS3 pin map |
| Speaker control function | `PYG3_SPK_Pulse` through M5PM1 | M5Stack StickS3 pin map |
| Audio/L3B control function | `PYG2_L3B_EN` through M5PM1 | M5Stack StickS3 pin map |
| IMU interrupt function | `PYG4_IMU_INT` through M5PM1 | M5Stack StickS3 pin map |
| User key 1 | GPIO11 / `KEY1` | M5Stack StickS3 pin map |
| User key 2 | GPIO12 / `KEY2` | M5Stack StickS3 pin map |
| LCD MOSI | GPIO39 | M5Stack StickS3 pin map |
| LCD SCLK | GPIO40 | M5Stack StickS3 pin map |
| LCD RS/DC | GPIO45 | M5Stack StickS3 pin map |
| LCD CS | GPIO41 | M5Stack StickS3 pin map |
| LCD reset | GPIO21 | M5Stack StickS3 pin map |
| LCD backlight | GPIO38 | M5Stack StickS3 pin map |
| LCD controller/resolution | ST7789P3, 135x240 | M5Stack StickS3 documentation |
| IR TX | GPIO46 | M5Stack StickS3 pin map |
| IR RX | GPIO42 | M5Stack StickS3 pin map |

## Current firmware status

The repository previously described the StickS3 firmware as a Classic Bluetooth HFP microphone. That is not a valid StickS3 implementation because the StickS3 controller is ESP32-S3, and ESP32-S3 does not support Bluetooth Classic / BR/EDR. The legacy HFP source is retained as quarantined historical code and intentionally errors if selected until refreshed for a non-StickS3 target.

The current default firmware is a Bluetooth LE GATT PCM microphone application. It initializes status UI, initializes the shared ESP-IDF v6 I2C master bus as needed for the source-backed M5PM1 L3B/LCD power sequence, starts an onboard ST7789P3 135x240 debug dashboard when `CONFIG_APP_STATUS_UI_LCD` is enabled, skips the optional M5PM1 audio probe, configures the ESP32-S3 I2S standard driver for capture-only RX, initializes the ES8311 ADC-only profile, advertises as `M5StickS3-Mic`, exposes custom BLE service UUID `0xFFF0`, and sends framed 16 kHz, 16-bit mono PCM notifications on characteristic UUID `0xFFF1`. It does not enable I2S TX, does not unmute the ES8311 DAC, and does not pulse or enable the speaker amplifier.

## Shared I2C bus

ES8311 (`0x18`), BMI270 (`0x68`), and M5PM1 (`0x6e`) share the GPIO47/GPIO48 I2C bus. `main/board_i2c.*` owns bus installation. Device drivers must not install the I2C driver independently.

## Audio clock profile

The current source-backed software profile is intentionally explicit:

- sample rate / LRCK: 16 kHz
- fixed MCLK: 12.288 MHz on GPIO18
- BCLK target: 512 kHz on GPIO17 for 16-bit mono capture
- LRCK/WS: GPIO15
- ES8311 clock register value used by the current project sequence: `0x1B`

For the ESP-IDF v6 standard I2S channel API path, the driver uses a 768 × Fs MCLK multiple to generate the 12.288 MHz ES8311 master clock for 16 kHz audio. Hardware acceptance must measure GPIO18, GPIO17, and GPIO15 before claiming physical audio success.

## M5PM1 L3B audio power sequence

The StickS3 documentation and schematic identify M5PM1/PY `G2` as `PYG2_L3B_EN`, with the ES8311 audio rail on `3V3_L3B_AU`. The default capture-only boot now enables that rail before ES8311 register access by configuring M5PM1 GPIO2 as a normal output, push-pull drive, and low output using the same tested GPIO-helper path as LCD/L3B setup. M5PM1 powers up in 100 kHz I2C mode, so the shared register-bus cache keeps the M5PM1 device handle at 100 kHz while other devices can use the board-default 400 kHz speed. The separate M5PM1 identity probe remains optional, but an L3B power-enable failure is fail-fast because ES8311 I2C access will not be reliable while the audio rail is unavailable.

This L3B enable sequence is not treated as evidence that local speaker output is safe. Speaker-amplifier pulse/control remains blocked and returns `ESP_ERR_NOT_SUPPORTED` until its exact source-backed M5PM1/AW8737 sequence is documented and tested.

## Button notes

Only GPIO11 (`KEY1`) and GPIO12 (`KEY2`) are treated as StickS3 user keys. GPIO35, GPIO37, and GPIO39 are not used as status buttons. GPIO39 is documented as LCD MOSI and must not be configured as a button.

The exact safe end-user gesture set is intentionally minimal until the physical relationship between `KEY1`, `KEY2`, and the side power/download behavior is verified. The current firmware polls the two documented keys, logs their presses, and displays per-key press counters on the optional LCD debug dashboard; product actions that require more than two safe gestures remain transport/UX decisions.

## LCD debug dashboard

The onboard LCD is documented as an ST7789P3 panel with 135x240 resolution. The firmware maps MOSI=GPIO39, SCLK=GPIO40, RS/DC=GPIO45, CS=GPIO41, RST=GPIO21, and BL=GPIO38, uses SPI3_HOST at 40 MHz, and applies the source-backed ST7789 controller RAM gap X=52/Y=40. `CONFIG_APP_STATUS_UI_LCD` defaults to enabled and makes `status_ui_init()` initialize the panel, enable the backlight, and render debug information: state, BLE service state, monitoring state, 16 kHz PCM rate, key press counters, uptime, and BLE device name. LCD bring-up is intentionally non-fatal so audio and BLE validation can continue if display hardware is unavailable or panel initialization fails.

## Audio output, speaker amplifier, and IR

The StickS3 includes an AW8737 speaker amplifier and M5PM1 speaker-control function (`PYG3_SPK_Pulse`). The exact M5PM1 command/register sequence for enabling and disabling the amplifier is not implemented here because it must be verified from official M5PM1 protocol documentation or source before driver code is written.

Speaker monitoring remains disabled as a product feature until a compatible transport requires local output and the M5PM1 speaker-control protocol is implemented. The StickS3 documentation notes that IR reception requires the speaker amplifier to be off.

## BMI270 scope

BMI270 is documented on the shared I2C bus at `0x68`, with interrupt routing through M5PM1. The current firmware does not initialize or use BMI270. This is intentional: no IMU-dependent feature is currently selected. M5PM1 GPIO helpers must preserve unrelated GPIO fields so future IMU interrupt routing is not clobbered by audio-related changes.

## Audio init failure policy

`board_audio_init_with_ops()` initializes in this order: shared I2C, optional M5PM1 probe, required source-backed audio power enable, I2S profile, ES8311 profile. On failure, later steps are skipped and the cleanup hook is called. The production cleanup policy logs the failure and does not guess a power-disable sequence because no source-backed L3B disable sequence exists yet.

## Unknowns / deferred decisions

- Replacement audio transport for StickS3.
- Exact M5PM1 speaker amplifier command sequence.
- Final safe button gesture UX beyond detecting `KEY1` and `KEY2` presses.
- Whether BMI270 is needed by any future product feature.

## Source references

- M5Stack StickS3 documentation and pin map: https://docs.m5stack.com/en/core/StickS3
- M5Stack M5GFX StickS3 initialization source: https://github.com/m5stack/M5GFX/blob/master/src/M5GFX.cpp
- StickS3 schematic PDF: https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150_Stick_S3_PRJ_V0.6_20251111_2025_11_17_16_10_24.pdf
- ES8311 datasheet: https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/products/atom/Atomic%20Echo%20Base/ES8311.pdf
- BMI270 datasheet: https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/core/K128%20CoreS3/BMI270.PDF
- ESP32-S3 technical reference manual: https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/477/esp32-s3_technical_reference_manual_cn.pdf
- Espressif Bluetooth architecture documentation: https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/bt-architecture/overview.html
