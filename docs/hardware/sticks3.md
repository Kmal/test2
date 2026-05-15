# M5Stack StickS3 hardware facts

This document records the StickS3 hardware facts that the firmware is allowed to rely on. Keep this file, `docs/hardware/sticks3.board.json`, and `main/board_sticks3.h` synchronized for board facts. Keep product behavior and user-facing firmware instructions in `docs/README.md`.

## Evidence map

| Item | Value | Source |
| --- | --- | --- |
| Controller | ESP32-S3-PICO-1-N8R8 | M5Stack StickS3 documentation |
| Flash | 8 MB | M5Stack StickS3 documentation |
| Bluetooth capability relevant to this project | ESP32-S3 supports Bluetooth LE, not Bluetooth Classic / BR/EDR | Espressif ESP32-S3 Bluetooth documentation |
| Audio codec | ES8311 | M5Stack StickS3 documentation and schematic |
| ES8311 I2C address | `0x18` | M5Stack StickS3 pin map |
| ES8311 MCLK | GPIO18 | M5Stack StickS3 pin map |
| ESP32-S3 I2S speaker/DAC data (`G14_I2S_DDAC`) | GPIO14 | StickS3 schematic and M5Unified StickS3 speaker config |
| ES8311 BCLK | GPIO17 | M5Stack StickS3 pin map |
| ES8311 LRCK / WS | GPIO15 | M5Stack StickS3 pin map |
| ESP32-S3 I2S microphone/ADC data (`G16_I2S_DADC`) | GPIO16 | StickS3 schematic and M5Unified StickS3 microphone config |
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

## Firmware pin mapping

`main/board_sticks3.h` is the authoritative firmware header for board-specific pins and hardware constants. This table keeps the removed README pin map in the hardware reference so board wiring, directions, and firmware constants stay documented in one hardware-focused place.

| Signal/function | ESP32-S3 GPIO/address | Direction | Firmware constant | Notes |
| --- | ---: | --- | --- | --- |
| SoC/module | ESP32-S3-PICO-1-N8R8 | N/A | N/A | ESP32-S3 does not support Bluetooth Classic / BR/EDR. |
| I2S MCLK | GPIO18 | ESP32-S3 -> ES8311 | `BOARD_I2S_MCLK_IO` | Current profile uses fixed MCLK at 12.288 MHz. |
| I2S BCLK | GPIO17 | ESP32-S3 -> ES8311 | `BOARD_I2S_BCK_IO` | Current documented target is 512 kHz for 16 kHz, 16-bit mono capture. |
| I2S LRCLK/WS | GPIO15 | ESP32-S3 -> ES8311 | `BOARD_I2S_WS_IO` | Current profile uses 16 kHz LRCK. |
| I2S TX / speaker DAC data (`G14_I2S_DDAC`) | GPIO14 | ESP32-S3 -> ES8311 | `BOARD_I2S_DO_IO` | Codec DAC/input data pin used by the speaker path. |
| I2S RX / microphone ADC data (`G16_I2S_DADC`) | GPIO16 | ES8311 -> ESP32-S3 | `BOARD_I2S_DI_IO` | Microphone/ADC samples read by the ESP32-S3. |
| I2C SDA | GPIO47 | Bidirectional | `BOARD_I2C_SDA_IO` | Shared ES8311/BMI270/M5PM1 control bus. |
| I2C SCL | GPIO48 | ESP32-S3 -> devices | `BOARD_I2C_SCL_IO` | Shared bus clock; M5PM1 handle remains at 100 kHz for power-up behavior. |
| ES8311 I2C address | `0x18` | N/A | `BOARD_ES8311_ADDR` | Minimal codec driver target. |
| BMI270 I2C address | `0x68` | N/A | `BOARD_BMI270_ADDR` | Used by the polling-only `bmi270.motion` hardware fact service when `CONFIG_APP_BMI270_FACTS=y`; interrupt routing remains unused. |
| M5PM1 I2C address | `0x6e` | N/A | `BOARD_M5PM1_ADDR` | Used for source-backed L3B audio rail and LCD power sequence. |
| User key 1 | GPIO11 | Input | `BOARD_BUTTON_KEY1_GPIO` | Official StickS3 `KEY1`, active-low with pull-up; cycles display pages and emits automation facts. |
| User key 2 | GPIO12 | Input | `BOARD_BUTTON_KEY2_GPIO` | Official StickS3 `KEY2`, active-low with pull-up; cycles app modes and emits automation facts. |
| LCD MOSI | GPIO39 | ESP32-S3 -> ST7789P3 | `BOARD_LCD_MOSI_GPIO` | Must not be configured as a status button or user GPIO. |
| LCD SCLK | GPIO40 | ESP32-S3 -> ST7789P3 | `BOARD_LCD_SCLK_GPIO` | SPI clock for the onboard 135x240 LCD. |
| LCD RS/DC | GPIO45 | ESP32-S3 -> ST7789P3 | `BOARD_LCD_DC_GPIO` | Display data/command select. |
| LCD CS | GPIO41 | ESP32-S3 -> ST7789P3 | `BOARD_LCD_CS_GPIO` | Display chip select. |
| LCD reset | GPIO21 | ESP32-S3 -> ST7789P3 | `BOARD_LCD_RST_GPIO` | Display reset. |
| LCD backlight | GPIO38 | ESP32-S3 -> LCD backlight | `BOARD_LCD_BL_GPIO` | Active-high backlight enable. |
| IR TX | GPIO46 | ESP32-S3 -> IR LED | `BOARD_IR_TX_GPIO` | NEC IR send action uses this RMT TX route after rule validation. |
| IR RX | GPIO42 | IR receiver -> ESP32-S3 | `BOARD_IR_RX_GPIO` | Reserved as documented IR RX; no receive action/source is wired. |

## Current firmware status

StickS3 uses an ESP32-S3 controller, and ESP32-S3 does not support Bluetooth Classic / BR/EDR. StickS3 can advertise and connect with Bluetooth LE services only; it cannot pair as a Classic Bluetooth audio endpoint, headset, speaker, serial-port, or other BR/EDR profile. No unsupported Bluetooth audio transport source or Kconfig option remains in this project.

For product behavior, user-facing firmware functions, and transport/API status, use `docs/README.md`. This hardware file records the board-level implications of the current default firmware: the onboard ST7789P3 135x240 LCD path is optional and non-fatal, the shared ESP-IDF v6 I2C master bus owns ES8311/BMI270/M5PM1 access, and the ES8311/I2S path is initialized only in a capture-only profile. The firmware does not drive I2S TX, unmute the ES8311 DAC, or pulse/enable the AW8737 speaker amplifier.

### Code-derived hardware implementation overlay

| Hardware-facing feature | Current code status | What that means |
| --- | --- | --- |
| LCD power and panel initialization | ✅ Implemented/wired when `CONFIG_APP_STATUS_UI_LCD=y` | `status_ui_init()` initializes the shared I2C bus, calls the M5PM1 LCD/L3B power helper, initializes the ST7789P3 panel, enables backlight GPIO38, and treats LCD failure as non-fatal. |
| Two StickS3 keys | ✅ Implemented/wired | Only GPIO11/KEY1 and GPIO12/KEY2 are polled as user keys; they drive menu navigation and button automation facts. |
| M5PM1 L3B helper | ✅ Wired for LCD/audio | The source-backed GPIO2-active-low sequence exists and is used by LCD power and by demand-driven ES8311 capture initialization. |
| Capture-only I2S/ES8311 profile | ✅ Implemented/wired by default | `board_audio_init_with_ops()` sequences I2C, optional M5PM1 probe, required audio power, I2S, and ES8311 ADC-only setup because `CONFIG_APP_SOUND_LEVEL_TRIGGERS=y` is set in the project defaults. |
| ES8311 microphone capture path | ✅ Implemented/wired by default | The default `CONFIG_APP_SOUND_LEVEL_TRIGGERS=y` build initializes the ES8311/I2S path only while firmware demand requires microphone metrics, reads normalized I2S microphone samples, and keeps the path capture-only. |
| I2S TX / onboard speaker output | ✅ Implemented/Kconfig-gated | StickS3 has an AW8737-backed speaker path; `speaker_tone` is Kconfig-gated and drives `G14_I2S_DDAC` with the M5PM1 PYG3 amplifier enabled only during playback. |
| IR send | ✅ Implemented/wired for actions | NEC IR actions use the RMT TX path on GPIO46 after rule validation. |
| IR receive | ⛔ Not implemented | GPIO42 is reserved as IR RX, but no receive action/source is wired. |
| BMI270 | ✅ Implemented/Kconfig-gated | `bmi270.motion` uses polling-only accelerometer reads when `CONFIG_APP_BMI270_FACTS=y`; interrupt routing through M5PM1 GPIO4 remains unused. |
| External GPIO digital/edge | ✅ Implemented/wired with validation | GPIO rules are allowed only after conflict checks reject LCD, I2C, I2S/audio, buttons, IR, boot/USB, and internal-risk pins. |
| GPIO pulse/frequency and HAT devices | ⛔ Not implemented / fail-closed | Capability validation reports these sources/actions disabled until source-backed drivers and routing are added. |
| Battery percent fact | 🟡 Implemented/Kconfig-gated; hardware calibration/bench check needed | M5PM1-backed `power.battery_percent` is emitted by `hardware_fact_service` only when `CONFIG_APP_BATTERY_FACTS=y`; the firmware LiPo interpolation curve still requires hardware comparison against the official `M5.Power` battery API. |
| USB/external-power fact | 🟡 Implemented/Kconfig-gated; hardware calibration/bench check needed | M5PM1-backed `power.usb_present` is emitted by `hardware_fact_service` only when `CONFIG_APP_USB_POWER_FACTS=y`; VIN/5V reads are independent of VBAT availability; the fact is emitted only when USB presence is proven or both VIN and 5V reads prove absence, and the firmware does not change EXT_5V mode. |
| Safe ADC voltage facts | 🟡 Implemented/Kconfig-gated; hardware calibration/bench check needed | ADC1-only `adc.voltage_mv` is emitted by `hardware_fact_service` only when `CONFIG_APP_ADC_FACTS=y`; exposure is limited to the safe Grove/Hat ADC1 allowlist pending hardware calibration/bench checks. |

## Shared I2C bus

ES8311 (`0x18`), BMI270 (`0x68`), and M5PM1 (`0x6e`) share the GPIO47/GPIO48 I2C bus. `main/board_i2c.*` owns bus installation. Device drivers must not install the I2C driver independently.

## Audio clock profile

The default capture-only software profile is intentionally explicit when the sound-level audio path is called:

- sample rate / LRCK: 16 kHz
- fixed MCLK: 12.288 MHz on GPIO18
- BCLK target: 512 kHz on GPIO17 for 16-bit mono capture
- LRCK/WS: GPIO15
- ES8311 12.288 MHz / 16 kHz clock-manager register-2 value used by the current project sequence: `0x40`

For the ESP-IDF v6 standard I2S channel API path, the driver uses a 768 × Fs MCLK multiple to generate the 12.288 MHz ES8311 master clock for 16 kHz audio and sets a 32-bit slot width for the physical BCLK target while reading DMA data as 16-bit samples according to `I2S_DATA_BIT_WIDTH_16BIT`. Hardware acceptance must measure GPIO18, GPIO17, and GPIO15 before claiming physical audio success. The default `CONFIG_APP_SOUND_LEVEL_TRIGGERS=y` build calls this initializer for capture-only microphone metrics.

## M5PM1 L3B audio power sequence

The StickS3 documentation and schematic identify M5PM1/PY `G2` as `PYG2_L3B_EN`, with the ES8311 audio rail on `3V3_L3B_AU`. The implemented helper configures M5PM1 GPIO2 as a normal output, push-pull drive, and **low** output using the same tested GPIO-helper path as LCD/L3B setup. The official M5PM1 StickS3 guidance enables L3B with `gpioSetOutput(..., false)`, after which this firmware writes `I2C_CFG=0x00` and waits for the rail before LCD reset. M5PM1 powers up in 100 kHz I2C mode, so the shared register-bus cache keeps the M5PM1 device handle at 100 kHz while other devices can use the board-default 400 kHz speed. The PMIC helper retries a first `ESP_ERR_INVALID_RESPONSE` once, because the boot log showed the LCD path failing on the first M5PM1 GPIO-function read while a later L3B access succeeded. The separate M5PM1 identity probe remains optional. In the default boot, this sequence is reached through LCD power initialization when LCD is enabled and through demand-driven sound-level audio initialization when enabled `sound.*` rules or Web UI telemetry demand exist.

This L3B enable sequence is separate from speaker amplification. The Kconfig-gated `speaker_tone` action uses the source-backed M5PM1 PYG3 SPK amplifier sequence (`GPIO3` function, output mode, push-pull drive, high to enable and low to disable) and releases playback audio resources after each tone so demand-driven microphone capture can restart.

## Button notes

Only GPIO11 (`KEY1`) and GPIO12 (`KEY2`) are treated as StickS3 user keys. GPIO35, GPIO37, and GPIO39 are not used as status buttons. GPIO39 is documented as LCD MOSI and must not be configured as a button.

The current firmware polls the two documented keys, logs their presses, displays per-key press counters on the optional LCD dashboard, and emits normalized automation facts. In the menu, KEY1 short selects, KEY2 short moves next, KEY2 double moves previous, KEY2 long goes back, and KEY1 long from the idle status view opens `Main`. Product actions that require more than the two documented keys remain transport/UX decisions.

## LCD debug dashboard

The onboard LCD is documented as an ST7789P3 panel with 135x240 resolution. The firmware maps MOSI=GPIO39, SCLK=GPIO40, RS/DC=GPIO45, CS=GPIO41, RST=GPIO21, and BL=GPIO38, uses SPI3_HOST at 40 MHz, and applies the source-backed ST7789 controller RAM gap X=52/Y=40. `CONFIG_APP_STATUS_UI_LCD` defaults to enabled and makes `status_ui_init()` initialize the panel, enable the backlight, and render the launcher/menu UI with Wi-Fi, BLE status, automation, and settings screens. LCD bring-up is intentionally non-fatal so Wi-Fi, BLE, and automation validation can continue if display hardware is unavailable or panel initialization fails.

When validating the L3B/LCD fix on UART, the expected M5PM1 lines include `active_level=low`, `out_reg=0x11`, `out_value=0x00`, and a later GPIO output update with `value=0x00`. If the UART still reports `active_level=high` or `GPIO2 driven high`, the board is running an older application image and must be rebuilt/reflashed before judging the LCD or ES8311 result.

## Audio output, speaker amplifier, and IR

The StickS3 includes an ES8311 codec, MEMS microphone, AW8737 speaker amplifier, and M5PM1 speaker-control function (`PYG3_SPK_Pulse`). The implemented audio-output scope is intentionally narrow: the Kconfig-gated `speaker_tone` action uses a bounded 16 kHz square-tone generator on the playback-only ES8311/I2S path and schematic net `G14_I2S_DDAC`, configures M5PM1 GPIO3 as a normal push-pull output, drives it high only while tone PCM is being written, then drives it low and releases playback resources. This is not a general M5Unified speaker playback API; broader sample rates, streaming, gain modes, and arbitrary PCM playback remain out of scope. The optional `board_speaker_amp_pulse()` gain/pulse helper remains fail-closed because no feature currently needs AW8737 pulse-mode gain control.

The official StickS3 microphone example states that the microphone and speaker cannot be used at the same time. Firmware therefore stops demand-driven microphone capture before a speaker action, starts `BOARD_AUDIO_PROFILE_PLAYBACK_ONLY`, and resynchronizes sound-level demand after playback so `BOARD_AUDIO_PROFILE_CAPTURE_ONLY` can restart if rules or Web UI telemetry still need it. The official product page also recommends speaker volume below 75% when running from battery; both rule validation and `action_speaker` validation cap `speaker_volume_percent` at `74` so integer values stay below 75%.

The StickS3 documentation notes that IR reception requires the speaker amplifier to be off. This firmware still implements only IR transmit actions, but the speaker action disables PYG3 after every tone and on playback-write failures so future IR RX work starts from an amplifier-off policy.

## BMI270 scope

BMI270 is documented on the shared I2C bus at `0x68`, with interrupt routing through M5PM1. The current firmware uses BMI270 only for polling-based `bmi270.motion` facts when `CONFIG_APP_BMI270_FACTS=y`; it does not enable BMI270 interrupt mode and does not modify the M5PM1 IMU interrupt GPIO. M5PM1 GPIO helpers must preserve unrelated GPIO fields so future IMU interrupt routing is not clobbered by audio-related changes.

## Audio init failure policy

`board_audio_init_with_ops()` initializes in this order in the default `CONFIG_APP_SOUND_LEVEL_TRIGGERS=y` build when shared sound demand requires monitoring (enabled `sound.*` rules or Web UI telemetry): shared I2C, optional M5PM1 probe, required source-backed audio power enable, I2S profile, ES8311 profile. On failure, later steps are skipped and the cleanup hook is called. The production cleanup policy logs the failure and does not guess a power-disable sequence because no source-backed L3B disable sequence exists yet. Maintainers can still turn the Kconfig option off for audio-free builds.

## Automation hardware scope

Safe GPIO digital and edge triggers are implemented only behind conflict validation. Pins used by LCD, I2C, I2S/audio, the two StickS3 buttons, IR, boot/USB, or internal-risk functions are rejected before a rule can be saved. Battery percent, USB/external-power present, BMI270 motion, and safe ADC1 facts are implemented behind separate Kconfig gates. HAT sensors/actions and GPIO pulse/frequency facts remain disabled until their hardware behavior and routing are verified.

## Unknowns / deferred decisions

- Hardware bench validation of the default sound-level capture path, including measured I2S clocks and ES8311 sample alignment; the firmware still does not expose PCM streaming.
- Hardware bench validation of the separately Kconfig-gated battery, USB/external-power, BMI270 polling, and ADC facts.
- Whether HAT features are needed by a future product feature.

## Source references

- M5Stack StickS3 documentation and pin map: https://docs.m5stack.com/en/core/StickS3
- M5Stack StickS3 Arduino programming documentation: https://docs.m5stack.com/en/arduino/m5sticks3/program
- M5Stack StickS3 Battery Arduino documentation: https://docs.m5stack.com/en/arduino/m5sticks3/battery
- M5Stack StickS3 Button Arduino documentation: https://docs.m5stack.com/en/arduino/m5sticks3/button
- M5Stack StickS3 Display Arduino documentation: https://docs.m5stack.com/en/arduino/m5sticks3/display
- M5Stack StickS3 IMU Arduino documentation: https://docs.m5stack.com/en/arduino/m5sticks3/imu
- M5Stack StickS3 IR NEC Arduino documentation: https://docs.m5stack.com/en/arduino/m5sticks3/ir_nec
- M5Stack StickS3 Microphone Arduino documentation: https://docs.m5stack.com/en/arduino/m5sticks3/mic
- M5Stack StickS3 Speaker Arduino documentation: https://docs.m5stack.com/en/arduino/m5sticks3/speaker
- M5Stack StickS3 Wakeup Arduino documentation: https://docs.m5stack.com/en/arduino/m5sticks3/wakeup
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


## Sound-level trigger reference review

Shared sound-capture work requires an upstream/vendor reference review before changing audio code. For this implementation, the ESP-IDF programming guide was used for component/Kconfig conventions, FreeRTOS task/semaphore use, logging/error style, and the channel-based I2S API assumptions. M5Stack StickS3 product and Arduino/M5PM1 documentation, the StickS3 schematic, M5PM1, M5Unified, and M5GFX sources were cross-checked for the ESP32-S3-PICO-1-N8R8 board identity, ES8311 mono codec, MEMS microphone, AW8737 amplifier, MCLK/BCLK/LRCK/DADC/DDAC pins, shared I2C addresses, M5PM1 L3B power behavior, PYG3 speaker-amplifier behavior, and LCD/L3B safety. The ES8311 datasheet was used for ADC/I2S-format expectations, MCLK/LRCK clocking, and capture-only codec setup. The BMI270 datasheet was reviewed only to avoid altering the shared I2C/interrupt behavior; sound-capture demand code does not initialize or change BMI270 state.

Shared sound capture uses `BOARD_AUDIO_PROFILE_CAPTURE_ONLY` for both enabled `sound.*` rules and Web UI telemetry demand. The speaker amplifier and I2S TX/DAC path remain disabled for that capture feature; the `speaker_tone` action follows the official M5Unified single-owner audio pattern by stopping microphone capture before starting `BOARD_AUDIO_PROFILE_PLAYBACK_ONLY`, then allowing demand-driven capture to restart afterwards.

### Speaker-action conformance review

| Official source/spec point | Firmware check result |
| --- | --- |
| Product documentation lists ES8311 audio codec, MEMS microphone, AW8737 power amplifier, and an 8Ω/1W speaker. | Firmware keeps ES8311 codec setup in `es8311.c`, M5PM1/PYG3 amplifier control in `board_audio_power.c`, and the bounded 16 kHz square-tone action in `action_speaker.c`; it does not expose generic speaker streaming/playback. |
| StickS3 schematic names GPIO14 as `G14_I2S_DDAC` and GPIO16 as `G16_I2S_DADC`; M5Unified uses GPIO14 for speaker data out and GPIO16 for microphone data in. | `BOARD_I2S_DO_IO=14` is used only by playback profiles and `BOARD_I2S_DI_IO=16` is used only by capture profiles. |
| M5Unified initializes StickS3 PYG3 as a normal GPIO output and toggles M5PM1 GPIO output bit 3 for speaker enable/disable. | `board_speaker_amp_set()` writes GPIO3 function, output mode, push-pull drive, and output high/low with read-modify-write preservation of unrelated PMIC bits. |
| Official microphone example says mic and speaker cannot be used simultaneously. | `app_send_speaker_rule_action()` stops the sound-level service before playback and calls `app_sound_level_sync()` afterwards. |
| Product speaker-volume notice recommends keeping battery speaker volume below 75%. | Rule and action validation reject volume `0` and values above `74`; default web-import value is `50`. |
| Product IR note says IR reception requires speaker amplifier off. | IR receive remains unimplemented; the speaker action always disables PYG3 after success or failed writes. |

## Hardware automation facts

The firmware exposes the following StickS3 hardware facts when their Kconfig gates are enabled. These names match the rule source metadata returned by the capability registry. The implementation is source-aligned with the official StickS3 pin map, Arduino examples, M5PM1 guidance, and ESP32-S3 ADC capability, but it is **not** a full product conformance claim until the facts are validated on real StickS3 hardware.

### Official-source conformance boundary

| Fact area | Official source point | Firmware boundary |
| --- | --- | --- |
| Battery percent | The StickS3 battery example exposes charging status, battery level, and battery voltage through `M5.Power`. | Firmware reads the M5PM1 VBAT millivolt registers directly and converts valid readings through its own LiPo interpolation curve; this is source-backed, but the exact percentage curve still needs physical comparison against M5Unified on StickS3. |
| USB/external power | The product page documents USB Type-C 5 V input and warns that EXT_5V/Grove/Hat2-Bus 5 V can be input or output depending on EXT_5V mode. | Firmware reads M5PM1 VIN and 5V voltage registers independently of VBAT availability and compares them with `CONFIG_APP_POWER_USB_PRESENT_MV`; it reports USB presence when either readable rail crosses the threshold, reports absence only when both rails read below the threshold, and does not switch EXT_5V output/input mode or infer current direction. |
| BMI270 motion | The product/pin-map docs place BMI270 at I2C address `0x68`, and the M5PM1 wakeup example routes BMI270 INT1 through M5PM1 GPIO4/PYG4. | Firmware intentionally implements polling-only software motion facts and leaves BMI270 interrupt mode plus M5PM1 PYG4/PYG1 wake routing untouched. |
| ADC voltage | The official pin map identifies Grove `G9`/`G10` and Hat2-Bus `G4`..`G8` expansion signals; ESP32-S3 ADC support is limited to ADC-capable GPIOs. | Firmware exposes only the safe ADC1 allowlist (`grove.g9`, `grove.g10`, `hat.g4`..`hat.g8`) and excludes boot, button, USB-JTAG, LCD, I2C, audio, IR, power-sensitive, and non-ADC pins. |


### Battery percent: `power.battery_percent`

* Source: M5PM1 VBAT millivolt registers on the shared I2C bus (`SDA=GPIO47`, `SCL=GPIO48`, PMIC address `0x6e`).
* Policy: VBAT samples are considered valid only in the `2500..4500 mV` range.
* Conversion: valid VBAT values are converted through the LiPo interpolation curve used by `board_power_lipo_percent_from_mv()`.
* Configuration: emitted only when `CONFIG_APP_BATTERY_FACTS=y`.

### USB/external power present: `power.usb_present`

* Source: M5PM1 VIN and 5V input/output millivolt reads.
* Policy: USB/external power is present when either VIN or 5V in/out is at least `CONFIG_APP_POWER_USB_PRESENT_MV`.
* The StickS3 documentation warns that the 5V interface can be input or output depending on EXT_5V mode; automation only reads voltage and does not change EXT_5V/L3B policy.
* Configuration: emitted only when `CONFIG_APP_USB_POWER_FACTS=y`.

### BMI270 motion: `bmi270.motion`

* Source: BMI270 accelerometer polling at I2C address `0x68`.
* First implementation: polling-only software thresholding using `CONFIG_APP_BMI270_MOTION_DELTA_MG` and a fixed still hysteresis of `30 mg`; initialization uses the BMI270 normal-power polling register path (`PWR_CONF=0x02`, accelerometer 100 Hz normal bandwidth, ±2g range, `PWR_CTRL.acc_en`).
* Interrupt deferral: the implementation does not enable BMI270 interrupt mode and does not modify M5PM1 GPIO4 / `PYG4_IMU_INT`.
* Future interrupt mode must preserve existing M5PM1 GPIO state and avoid disturbing M5PM1 GPIO2 / `PYG2_L3B_EN` LCD/audio power behavior.
* Configuration: emitted only when `CONFIG_APP_BMI270_FACTS=y`.

### ADC voltage: `adc.voltage_mv`

* Source: ESP-IDF ADC oneshot on ESP32-S3 ADC1 only; ADC2 is not exposed.
* Safe source keys:
  * `grove.g9`
  * `grove.g10`
  * `hat.g4`
  * `hat.g5`
  * `hat.g6`
  * `hat.g7`
  * `hat.g8`
* Explicit exclusions:
  * `GPIO1`, `GPIO2`, and `GPIO3` are not exposed by default because they overlap boot/power-sensitive StickS3 Hat2-Bus roles.
  * `GPIO11` and `GPIO12` are not exposed because they are user buttons.
  * `GPIO19` and `GPIO20` are not exposed because ESP-IDF uses them for USB-JTAG by default on ESP32-S3.
  * `GPIO43` and `GPIO44` are not exposed as ADC because they are not ESP32-S3 ADC-capable GPIOs.
  * LCD, I2C, audio, IR, and other internal-risk pins are not exposed.
* Configuration: emitted only when `CONFIG_APP_ADC_FACTS=y`.

### Implementation references

* StickS3 product documentation and Arduino programming docs provide the shared I2C pins, BMI270/M5PM1 addresses, Grove/Hat2-Bus pin map, buttons, LCD, audio, and IR exclusions.
* M5PM1 Arduino docs and source provide the source-verified PMIC voltage register layout used for VBAT, VIN, VREF, and 5V in/out reads; the firmware follows the upstream little-endian 16-bit voltage read helper so USB/VIN values above 4095 mV remain representable.
* The BMI270 datasheet provides chip ID and accelerometer data register semantics for the polling driver.
* The ESP-IDF Programming Guide provides the ADC oneshot and calibration API assumptions and ESP32-S3 ADC channel mapping.
