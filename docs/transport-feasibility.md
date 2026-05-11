# StickS3 transport feasibility decision

## Current decision

The selected product behavior for this repository is a **custom Bluetooth LE sound-meter and local automation device**. The firmware advertises as `M5StickS3-Meter`, exposes custom service UUID `0xFFF0`, publishes sound telemetry on `0xFFF2`, accepts control writes on `0xFFF3`, exposes status on `0xFFF4`, can optionally stream raw PCM debug frames on `0xFFF1`, and publishes automation rule-event notifications on `0xFFF5`.

This is functional for validation and custom host applications, but it is **not** Bluetooth Classic HFP, BLE Audio, USB Audio, or an operating-system-native microphone class.

## Why Classic Bluetooth HFP is rejected

StickS3 uses ESP32-S3-PICO-1-N8R8. ESP32-S3 does not support Bluetooth Classic / BR/EDR, so Classic Bluetooth HFP is not a valid StickS3 transport. The legacy HFP source remains quarantined behind `CONFIG_APP_TRANSPORT_HFP_LEGACY`, and Kconfig blocks it on `esp32s3`.

## Candidate matrix

| Candidate | StickS3 status | Notes |
| --- | --- | --- |
| Classic Bluetooth HFP | Rejected | ESP32-S3 does not support Bluetooth Classic / BR/EDR. |
| Custom BLE sound-level telemetry | Selected | Current implementation; requires a BLE central/custom host and does not appear as an OS microphone. |
| Custom BLE GATT PCM/control | Debug only | Raw PCM debug notifications exist for diagnostics, not as a product microphone class. |
| USB Audio device | Deferred | Must be verified against ESP-IDF/TinyUSB support and a wired-product requirement. |
| BLE Audio | Deferred | Must not be selected until official support, roles, memory, and host compatibility are verified. |
| Retarget to Classic-BT-capable ESP32 board | Product change | Only valid if the product stops being StickS3. |

## Local speaker output

Local speaker monitoring remains disabled. The default profile is capture-only: no I2S TX, no ES8311 DAC path, and no AW8737/M5PM1 speaker-amplifier pulse. Speaker output can only be enabled after the exact M5PM1/AW8737 control sequence is source-backed, implemented, tested, and reflected in the hardware safety checks.

## Product criteria for future transport changes

Before replacing the selected transport, decide and document:

- whether the device must appear as a standard OS microphone;
- whether audio must be wireless;
- whether a custom host application is acceptable;
- required host platforms;
- latency and bandwidth expectations;
- whether local speaker monitoring is required;
- pairing, provisioning, discovery, and security UX.
