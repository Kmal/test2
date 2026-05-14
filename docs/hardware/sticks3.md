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

The current default firmware is a local configuration and automation device. From the current code path it initializes NVS/network services, Wi-Fi station/setup-AP support, the rule runtime, the on-demand Web UI server lifecycle, status UI, and the shared ESP-IDF v6 I2C master bus when the LCD/power helper needs it. It starts an onboard ST7789P3 135x240 launcher/menu UI when `CONFIG_APP_STATUS_UI_LCD` is enabled, advertises as `M5StickS3-Control`, exposes custom BLE service UUID `0xFFF0`, exposes status on characteristic UUID `0xFFF4`, and emits `M5RE` automation rule events on characteristic UUID `0xFFF5`. The default firmware does not call `board_audio_init()`, does not initialize I2S audio, does not start sound-level telemetry, does not enable I2S TX, does not unmute the ES8311 DAC, and does not pulse or enable the speaker amplifier.

### Code-derived hardware implementation overlay

| Hardware-facing feature | Current code status | What that means |
| --- | --- | --- |
| LCD power and panel initialization | ✅ Implemented/wired when `CONFIG_APP_STATUS_UI_LCD=y` | `status_ui_init()` initializes the shared I2C bus, calls the M5PM1 LCD/L3B power helper, initializes the ST7789P3 panel, enables backlight GPIO38, and treats LCD failure as non-fatal. |
| Two StickS3 keys | ✅ Implemented/wired | Only GPIO11/KEY1 and GPIO12/KEY2 are polled as user keys; they drive menu navigation and button automation facts. |
| M5PM1 L3B helper | 🟡 Helper path plus LCD use | The source-backed GPIO2-high sequence exists and is used by LCD power; the audio-specific `board_audio_power_enable()` helper also exists, but default boot does not invoke the audio initializer. |
| Capture-only I2S/ES8311 profile | 🟡 Helper path only | `board_audio_init_with_ops()` can sequence I2C, optional M5PM1 probe, required audio power, I2S, and ES8311 ADC-only setup, but `app_main()` does not call it. |
| Sound metrics from microphone | ⛔ Not wired by default | Audio metric helpers exist, but no default task reads I2S samples and feeds sound facts into the rule runtime. |
| I2S TX / local speaker output | ⛔ Not implemented | Full-duplex/TX is not selected by default, and AW8737/M5PM1 speaker-amplifier control remains blocked with `ESP_ERR_NOT_SUPPORTED`. |
| IR send | ✅ Implemented/wired for actions | NEC IR actions use the RMT TX path on GPIO46 after rule validation. |
| IR receive | ⛔ Not implemented | GPIO42 is reserved as IR RX, but no receive action/source is wired. |
| BMI270 | ⛔ Not implemented | Address and routing are documented, but no IMU initialization or automation fact producer is wired. |
| External GPIO digital/edge | ✅ Implemented/wired with validation | GPIO rules are allowed only after conflict checks reject LCD, I2C, I2S/audio, buttons, IR, boot/USB, and internal-risk pins. |
| GPIO pulse/frequency, ADC, battery/power automation facts, HAT devices | ⛔ Not implemented / fail-closed | Capability validation reports these sources/actions disabled until source-backed drivers and routing are added. |

## On-device UI hierarchy

The onboard UI exposes:
- Web UI
- Connect to Wi-Fi
- Connect to Bluetooth
- All automations
- Settings

The menu status bar shows 24-hour time on the left and battery percentage on the right when available. Text entry uses a bottom 9-key input overlay.

## Shared I2C bus

ES8311 (`0x18`), BMI270 (`0x68`), and M5PM1 (`0x6e`) share the GPIO47/GPIO48 I2C bus. `main/board_i2c.*` owns bus installation. Device drivers must not install the I2C driver independently.

## Audio clock profile

The optional capture-only software profile is intentionally explicit when the audio helper path is called:

- sample rate / LRCK: 16 kHz
- fixed MCLK: 12.288 MHz on GPIO18
- BCLK target: 512 kHz on GPIO17 for 16-bit mono capture
- LRCK/WS: GPIO15
- ES8311 12.288 MHz / 16 kHz clock-manager register-2 value used by the current project sequence: `0x40`

For the ESP-IDF v6 standard I2S channel API path, the driver uses a 768 × Fs MCLK multiple to generate the 12.288 MHz ES8311 master clock for 16 kHz audio. Hardware acceptance must measure GPIO18, GPIO17, and GPIO15 before claiming physical audio success. This is not a default-boot claim: the current `app_main()` path does not call the audio initializer.

## M5PM1 L3B audio power sequence

The StickS3 documentation and schematic identify M5PM1/PY `G2` as `PYG2_L3B_EN`, with the ES8311 audio rail on `3V3_L3B_AU`. The implemented helper configures M5PM1 GPIO2 as a normal output, push-pull drive, and **high** output using the same tested GPIO-helper path as LCD/L3B setup. The current M5Stack M5GFX StickS3 initialization source drives `PYG2` high to enable L3B, writes `I2C_CFG=0x00`, and waits for the rail before LCD reset. M5PM1 powers up in 100 kHz I2C mode, so the shared register-bus cache keeps the M5PM1 device handle at 100 kHz while other devices can use the board-default 400 kHz speed. The PMIC helper retries a first `ESP_ERR_INVALID_RESPONSE` once, because the boot log showed the LCD path failing on the first M5PM1 GPIO-function read while a later L3B access succeeded. The separate M5PM1 identity probe remains optional. In the current default boot, this sequence is reached through LCD power initialization when LCD is enabled; audio-specific rail enable before ES8311 access exists in the optional audio initializer but is not called by `app_main()`.

This L3B enable sequence is not treated as evidence that local speaker output is safe. Speaker-amplifier pulse/control remains blocked and returns `ESP_ERR_NOT_SUPPORTED` until its exact source-backed M5PM1/AW8737 sequence is documented and tested.

## Button notes

Only GPIO11 (`KEY1`) and GPIO12 (`KEY2`) are treated as StickS3 user keys. GPIO35, GPIO37, and GPIO39 are not used as status buttons. GPIO39 is documented as LCD MOSI and must not be configured as a button.

The current firmware polls the two documented keys, logs their presses, displays per-key press counters on the optional LCD dashboard, and emits normalized automation facts. In the menu, KEY1 short selects, KEY2 short moves next, KEY2 double moves previous, KEY2 long goes back, and KEY1 long from the idle status view opens `Main`. Product actions that require more than the two documented keys remain transport/UX decisions.

## LCD debug dashboard

The onboard LCD is documented as an ST7789P3 panel with 135x240 resolution. The firmware maps MOSI=GPIO39, SCLK=GPIO40, RS/DC=GPIO45, CS=GPIO41, RST=GPIO21, and BL=GPIO38, uses SPI3_HOST at 40 MHz, and applies the source-backed ST7789 controller RAM gap X=52/Y=40. `CONFIG_APP_STATUS_UI_LCD` defaults to enabled and makes `status_ui_init()` initialize the panel, enable the backlight, and render the launcher/menu UI with Wi-Fi, BLE status, automation, and settings screens. LCD bring-up is intentionally non-fatal so Wi-Fi, BLE, and automation validation can continue if display hardware is unavailable or panel initialization fails.

When validating the L3B/LCD fix on UART, the expected M5PM1 lines include `active_level=high`, `out_reg=0x11`, `out_value=0x04`, and a later GPIO output update with `value=0x04`. If the UART still reports `active_level=low` or `GPIO2 driven low`, the board is running an older application image and must be rebuilt/reflashed before judging the LCD or ES8311 result.

## Audio output, speaker amplifier, and IR

The StickS3 includes an AW8737 speaker amplifier and M5PM1 speaker-control function (`PYG3_SPK_Pulse`). The exact M5PM1 command/register sequence for enabling and disabling the amplifier is not implemented here because it must be verified from official M5PM1 protocol documentation or source before driver code is written.

Speaker monitoring remains disabled as a product feature until a compatible transport requires local output and the M5PM1 speaker-control protocol is implemented. The StickS3 documentation notes that IR reception requires the speaker amplifier to be off.

## BMI270 scope

BMI270 is documented on the shared I2C bus at `0x68`, with interrupt routing through M5PM1. The current firmware does not initialize or use BMI270. This is intentional: no IMU-dependent feature is currently selected. M5PM1 GPIO helpers must preserve unrelated GPIO fields so future IMU interrupt routing is not clobbered by audio-related changes.

## Audio init failure policy

`board_audio_init_with_ops()` initializes in this order when an optional audio path calls it: shared I2C, optional M5PM1 probe, required source-backed audio power enable, I2S profile, ES8311 profile. On failure, later steps are skipped and the cleanup hook is called. The production cleanup policy logs the failure and does not guess a power-disable sequence because no source-backed L3B disable sequence exists yet. The current default `app_main()` path does not invoke this initializer.

## Automation hardware scope

Safe GPIO digital and edge triggers are implemented only behind conflict validation. Pins used by LCD, I2C, I2S/audio, the two StickS3 buttons, IR, boot/USB, or internal-risk functions are rejected before a rule can be saved. HAT sensors/actions, GPIO pulse/frequency, ADC, battery/power, and BMI270 facts remain disabled until their hardware behavior and routing are verified.

## Unknowns / deferred decisions

- Whether a future product requires optional audio capture; the default control firmware does not expose audio or sound-level telemetry.
- Exact M5PM1 speaker amplifier command sequence.
- Whether BMI270, ADC, battery, or HAT features are needed by a future product feature.

## Source references

- M5Stack StickS3 documentation and pin map: https://docs.m5stack.com/en/core/StickS3
- M5Stack StickS3 Arduino programming documentation: https://docs.m5stack.com/en/arduino/m5sticks3/program
- M5Stack StickS3 M5PM1 Arduino documentation: https://docs.m5stack.com/en/arduino/m5sticks3/m5pm1
- M5Stack M5PM1 source repository: https://github.com/m5stack/M5PM1
- M5Stack M5Unified source repository: https://github.com/m5stack/M5Unified
- M5Stack M5GFX source repository: https://github.com/m5stack/M5GFX
- M5Stack M5GFX StickS3 initialization source: https://github.com/m5stack/M5GFX/blob/master/src/M5GFX.cpp
- StickS3 schematic PDF: https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150_Stick_S3_PRJ_V0.6_20251111_2025_11_17_16_10_24.pdf
- ES8311 datasheet: https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/products/atom/Atomic%20Echo%20Base/ES8311.pdf
- BMI270 datasheet: https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/core/K128%20CoreS3/BMI270.PDF
- ESP32-S3 technical reference manual: https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/477/esp32-s3_technical_reference_manual_cn.pdf
- Espressif Bluetooth architecture documentation: https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/bt-architecture/overview.html
