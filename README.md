# M5Stack StickS3 board-support firmware

This repository contains ESP-IDF firmware for M5Stack StickS3 board bring-up, source-gated ES8311 audio-codec initialization, documented StickS3 key polling, and validation tooling.

## Current status

The previous project description claimed that StickS3 could expose a Classic Bluetooth Hands-Free Profile (HFP) microphone. That claim was incorrect for StickS3: the board uses ESP32-S3-PICO-1-N8R8, and ESP32-S3 does not support Bluetooth Classic / BR/EDR. Classic Bluetooth HFP is therefore not a valid StickS3 transport.

The default firmware selects `CONFIG_APP_TRANSPORT_NONE`, initializes documented StickS3 status peripherals, initializes the shared I2C bus, probes M5PM1, and brings up a capture-only audio profile for the ES8311. The capture-only profile keeps the ESP32-S3 I2S TX path, ES8311 DAC path, and speaker amplifier disabled. The legacy HFP source is quarantined behind `CONFIG_APP_TRANSPORT_HFP_LEGACY`, which is unavailable for `esp32s3`.

A replacement transport is intentionally deferred until product requirements choose between candidates such as USB Audio, Wi-Fi streaming, BLE custom service, BLE Audio if officially supported for the required ESP32-S3 role, or retargeting to Classic-Bluetooth-capable hardware. See `docs/transport-feasibility.md`.

## Hardware and pin mapping

`main/board_sticks3.h` is the authoritative source for board-specific pins and hardware constants, and `docs/hardware/sticks3.md` records the source-backed hardware facts.

| Signal/function | ESP32-S3 GPIO/address | Direction | Firmware constant | Notes |
| --- | ---: | --- | --- | --- |
| I2S MCLK | GPIO18 | ESP32-S3 -> ES8311 | `BOARD_I2S_MCLK_IO` | Current profile uses fixed MCLK at 12.288 MHz. |
| I2S BCLK | GPIO17 | ESP32-S3 -> ES8311 | `BOARD_I2S_BCK_IO` | Current documented target is 512 kHz for 16 kHz, 16-bit mono capture. |
| I2S LRCLK/WS | GPIO15 | ESP32-S3 -> ES8311 | `BOARD_I2S_WS_IO` | Current profile uses 16 kHz LRCK. |
| I2S TX / DAC data (`G14_I2S_DDAC`) | GPIO14 | ESP32-S3 -> ES8311 | `BOARD_I2S_DO_IO` | Physical DAC data pin; not driven in the no-transport capture-only profile. |
| I2S RX / ADC data (`G16_I2S_DADC`) | GPIO16 | ES8311 -> ESP32-S3 | `BOARD_I2S_DI_IO` | Microphone/ADC samples read by the ESP32-S3. |
| I2C SDA | GPIO47 | Bidirectional | `BOARD_I2C_SDA_IO` | Shared ES8311/BMI270/M5PM1 control bus. |
| I2C SCL | GPIO48 | ESP32-S3 -> devices | `BOARD_I2C_SCL_IO` | Shared bus clock, currently 400 kHz. |
| ES8311 I2C address | `0x18` | N/A | `BOARD_ES8311_ADDR` | Minimal codec driver target. |
| BMI270 I2C address | `0x68` | N/A | `BOARD_BMI270_ADDR` | Documented but intentionally unused by current firmware. |
| M5PM1 I2C address | `0x6e` | N/A | `BOARD_M5PM1_ADDR` | Probed by default; L3B/speaker writes remain blocked until source-backed polarity/sequence is verified. |
| User key 1 | GPIO11 | Input | `BOARD_BUTTON_KEY1_GPIO` | Official StickS3 `KEY1`, active-low with pull-up. |
| User key 2 | GPIO12 | Input | `BOARD_BUTTON_KEY2_GPIO` | Official StickS3 `KEY2`, active-low with pull-up. |
| LCD MOSI | GPIO39 | ESP32-S3 -> LCD | N/A | Must not be configured as a status button. |

The current audio clock profile is explicit: 16 kHz mono PCM, 16-bit samples, fixed MCLK at 12.288 MHz, documented BCLK target 512 kHz, and ES8311 clock register value `0x1B`. For the ESP-IDF legacy I2S driver path, fixed MCLK is the source of truth; `mclk_multiple` is driver bookkeeping and not treated as proof that MCLK equals 256 × Fs. Hardware acceptance must measure GPIO18/GPIO17/GPIO15 before claiming physical audio success.

## Buttons and status output

The status module polls only documented StickS3 keys: GPIO11 (`KEY1`) and GPIO12 (`KEY2`). GPIO35, GPIO37, and GPIO39 are not used as buttons. Pressing either key is logged, but no product action is assigned until a StickS3-compatible transport and safe UX are selected.

Status states are `booting`, `no transport selected`, `ready`, and `error`. The API in `main/status_ui.h` can later be backed by display hardware without changing board constants.

## Audio output, speaker amplifier, and IR

Local speaker monitoring is disabled in the default firmware. The StickS3 includes an AW8737 amplifier and M5PM1 speaker-control function (`PYG3_SPK_Pulse`), but the exact M5PM1 command sequence must be verified from official protocol/library sources before implementing speaker control. StickS3 documentation notes that IR reception requires the speaker amplifier to be off.

The code includes bit-preserving M5PM1 GPIO helpers and source-gated stubs for L3B audio power and speaker pulse control. Those stubs return `ESP_ERR_NOT_SUPPORTED` until the enable polarity and safe register sequence are verified; the no-transport boot path does not require those blocked writes.

## Source layout

* `main/main.c` contains StickS3 board bring-up and the no-transport runtime state.
* `main/board_audio.*` owns testable board-audio sequencing and dependency injection.
* `main/board_i2c.*` owns the single shared I2C bus for ES8311, M5PM1, and BMI270.
* `main/board_i2s.*` owns capture-only versus full-duplex I2S profiles.
* `main/board_audio_clock.*` documents the current fixed MCLK audio clock profile.
* `main/m5pm1.*` implements source-backed, bit-preserving M5PM1 probe/GPIO helpers.
* `main/board_audio_power.*` contains source-gated L3B and speaker-control stubs.
* `main/transport_hfp_legacy.*` quarantines the previous Classic Bluetooth HFP path; it is not available for StickS3/ESP32-S3.
* `main/es8311.*` implements the minimal ES8311 codec setup used by this firmware, including ADC-only and full-duplex profiles.
* `main/audio_resample.*` contains fixed-point audio resampling helpers retained for future transport work.
* `main/audio_pipeline.*` contains a host-testable transport-neutral PCM pipeline for future transports.
* `main/button_state.*` contains a host-testable two-key debounce/state helper.
* `main/status_ui.*` owns GPIO11/GPIO12 polling and logged status/feature state.
* `main/board_sticks3.h` is the board-specific pin and audio constant map.
* `config/sdkconfig.defaults` captures StickS3-safe SDK defaults with no Classic Bluetooth HFP enabled.
* `tools/check_*.py` validates board facts, transport config, audio clock/safety policy, and docs consistency.
* `tests/host/` contains host-side unit tests for pure C helpers and faked register/audio sequencing.

## Building and flashing

1. Install and activate ESP-IDF `v5.5.4` for ESP32-S3 development.
2. From the repository root, select the target and build:

   ```sh
   idf.py set-target esp32s3
   idf.py build
   ```

3. Flash and monitor a connected StickS3:

   ```sh
   idf.py -p <PORT> flash monitor
   ```

The monitor displays firmware logs tagged with `STICKS3_APP`, `BOARD_AUDIO`, `BOARD_I2C`, `BOARD_I2S`, `ES8311`, `M5PM1`, and `STATUS_UI`. It will state that no StickS3-compatible audio transport is selected.

## CI and local validation

Recommended local validation:

```sh
python3 tools/check_board_map.py
python3 tools/check_transport_config.py
python3 tools/check_docs_consistency.py
python3 tools/check_audio_clock.py
python3 tools/check_audio_safety.py
tests/host/run_host_tests.sh
idf.py set-target esp32s3
idf.py build
```

The local environment must provide ESP-IDF for the `idf.py` commands. Host tests do not require ESP-IDF.

## Limitations and further work

* No working StickS3 microphone transport is currently selected.
* Classic Bluetooth HFP is incompatible with StickS3/ESP32-S3.
* BLE Audio is not assumed feasible until official ESP-IDF documentation proves the exact ESP32-S3 support and required role.
* Speaker monitoring is disabled until M5PM1 speaker-amplifier control is source-backed and implemented.
* M5PM1 L3B audio power writes are blocked until the StickS3 polarity and safe sequence are verified.
* BMI270 is documented but unused.
* Hardware validation still requires a physical M5Stack StickS3.
