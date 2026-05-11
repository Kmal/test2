# StickS3 local rule automation status and plan

This document replaces the old phase checklist with the current implementation status. The firmware now contains a complete local rule automation foundation, plus a web UI and storage path, while external HAT/power/IMU features remain intentionally disabled until their hardware drivers are verified.

## Current implemented behavior

### Rule data model and validation

Implemented files: `main/rule_types.h`, `main/rule_types.c`, and validation coverage in host tests.

Current schema limits:

- `RULE_CONFIG_SCHEMA_VERSION = 1`
- `RULE_MAX_RULES = 8`
- `RULE_MAX_ACTIONS_PER_RULE = 3`
- `RULE_NAME_MAX = 32`
- `RULE_SOURCE_KEY_MAX = 32`
- `RULE_HTTP_URL_MAX = 128`
- `RULE_HTTP_AUTH_MAX = 96`
- cooldown range: 100 ms through 24 hours
- sustain range: 0 ms through 1 hour

Supported trigger sources today:

- `sound.rms_dbfs`
- `sound.peak_dbfs`
- `sound.clipped`
- `button.key1.short`
- `button.key2.short`
- `ble.connected`
- `wifi.connected`
- `gpio.digital`
- `gpio.edge`

Defined but disabled sources:

- battery percentage and USB-present facts;
- BMI270 motion;
- HAT PIR, ENV III, Ambient Light, ToF, NCIR, thermal, heart-rate, and HAT ADC sources;
- GPIO pulse count and GPIO frequency;
- generic ADC voltage.

Supported actions today:

- `ble_message`
- `http_post`
- `ir_send`
- `local_ui`

Defined but disabled action:

- `hat_operation`

### Rule engine

Implemented files: `main/rule_engine.h` and `main/rule_engine.c`.

The engine evaluates facts deterministically and returns rule events. It does not directly perform BLE, HTTP, HAT, IR, GPIO, I2C, or UI side effects. It implements:

- source/source-key matching;
- boolean and integer comparisons;
- false-to-true transition detection;
- sustain-duration tracking;
- cooldown enforcement;
- event sequencing and fire counts.

### Trigger adapters and runtime wiring

Implemented files include `main/trigger_sources.c`, `main/trigger_gpio.c`, and `main/rule_runtime.c`.

Current runtime fact inputs:

- sound metrics from the sound-meter analysis path;
- KEY1 and KEY2 short-press events from the status button path;
- BLE connection state polling;
- Wi-Fi network-ready state polling;
- GPIO digital and edge polling for validated safe pins.

GPIO safety validation rejects pins used by LCD, I2C, I2S/audio, StickS3 buttons, IR, boot/USB, and internal-risk functions. Supported GPIO profiles are `digital_high_low`, `debounced_contact`, `rising_edge`, and `falling_edge`.

### Capability registry

Implemented files: `main/capability_registry.h` and `main/capability_registry.c`.

The registry reports supported and disabled sources/actions through `/api/capabilities`. It is also used during rule validation so unsupported HAT/power/IMU/ADC/frequency features fail closed instead of being silently accepted.

### Action dispatch

Implemented files include `main/action_dispatcher.c`, `main/action_http.c`, `main/action_ir.c`, and BLE/local-UI adapters in `main/main.c`.

Current action behavior:

- BLE actions publish `M5RE` rule-event packets on characteristic `0xFFF5`.
- HTTP actions send a bounded JSON event with the configured URL/token/timeout when network is ready.
- IR actions send NEC frames through the configured RMT IR transmitter path.
- Local UI actions mark the service/UI ready for visible feedback.
- HAT actions return unsupported because HAT operation drivers are not verified.

### Web UI, Wi-Fi, and storage

Implemented files include `main/app_wifi.c`, `main/rule_web.c`, and `main/rule_config_store.c`.

Current behavior:

- Wi-Fi station/setup-AP support starts during boot when enabled.
- The LCD keyboard can gather Wi-Fi credentials before AP fallback when enabled.
- A compact web page is served at `/`.
- JSON endpoints expose status, capabilities, config import/export/save, Wi-Fi status/scan/connect/AP, rule testing, GPIO validation, and fail-closed HAT probing.
- Automation configs are saved as a bounded NVS blob and migrated/reset to safe defaults if invalid.

## Completed work summary

The original plan's core phases are complete in code and host tests:

1. Legacy HFP/microphone wording was removed from the product direction and the default transport was changed to a StickS3-compatible BLE sound meter.
2. Rule types, validation, and safe default config were added.
3. The deterministic rule engine was added.
4. Sound and button trigger facts were wired to the runtime.
5. Capability-gated external source/action validation was added.
6. GPIO digital/edge triggers were implemented behind safe-pin validation.
7. BLE, HTTP, IR, and local UI actions were implemented.
8. NVS storage and web configuration APIs were implemented.
9. Wi-Fi station/setup-AP provisioning and the compact web UI were implemented.
10. Main app integration now starts Wi-Fi, web/rule runtime, audio, BLE, sound meter, and background state tasks.
11. Host tests and static validation scripts cover the rule modules and major safety constraints.

## Remaining plan

### Required hardware validation

Before claiming production readiness, run the hardware smoke checklist on a physical StickS3:

- boot and reach the normal LCD UI;
- verify BLE `M5LM`, `M5TS`, and `M5RE` packets with a central;
- verify Wi-Fi station/setup AP and web endpoints;
- save a rule, reboot, and confirm NVS reload with masked secrets;
- validate sound metrics under quiet/loud input;
- validate GPIO digital/edge rules with an external fixture on safe pins;
- validate HTTP POST against a local test endpoint;
- validate NEC IR frames with an IR receiver or logic analyzer;
- measure MCLK/BCLK/LRCK and confirm capture-only audio behavior.

### Product hardening

Planned before user-facing deployment:

- improve the web editor UX beyond the current compact single-page/config JSON workflow;
- document network-security assumptions and add authentication or local-only deployment guidance if needed;
- add better status/error reporting for action results and failed rule saves;
- define a stable host-side BLE client example for telemetry/rule events.

### Deferred feature work

These features remain blocked until source-backed protocols and hardware validation exist:

- HAT sensor drivers and HAT action drivers;
- GPIO pulse count/frequency profiles;
- battery/USB/power facts;
- BMI270 motion facts;
- ADC voltage facts;
- speaker amplifier/local output;
- a standard USB Audio or BLE Audio transport.

## Non-goals for the current firmware

- It does not provide Classic Bluetooth HFP.
- It does not appear as a native OS microphone.
- It does not enable the StickS3 speaker amplifier.
- It does not initialize BMI270 or HAT sensors.
- It does not accept unsafe GPIO pins that conflict with board functions.
