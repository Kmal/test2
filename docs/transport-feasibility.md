# StickS3 transport feasibility decision

## Product criteria

A replacement transport must be selected before the firmware can honestly claim to be a working StickS3 microphone. The decision must answer:

- Must the device appear as a standard operating-system microphone without custom host software?
- Must audio be wireless?
- Is a custom host application acceptable?
- What host platforms are required?
- What latency and audio bandwidth are acceptable?
- Is local speaker monitoring required?
- What pairing, provisioning, or discovery UX is acceptable?

## Current decision

**Decision: Selected for this repository state.** The default StickS3-compatible product is a custom Bluetooth LE sound-level meter. The firmware advertises as `M5StickS3-Meter`, exposes custom BLE service UUID `0xFFF0`, sends compact RMS/peak/VU/clipping telemetry notifications on characteristic UUID `0xFFF2`, and keeps optional framed 16 kHz, 16-bit mono PCM debug notifications on characteristic UUID `0xFFF1`, one-byte control writes on `0xFFF3`, and status reads/notifications on `0xFFF4`. The Classic Bluetooth HFP path remains quarantined as legacy non-StickS3 code.
**Decision: Selected for this repository state.** The default StickS3-compatible product is a custom Bluetooth LE sound-level meter. The firmware advertises as `M5StickS3-Meter`, exposes custom BLE service UUID `0xFFF0`, sends compact RMS/peak/VU/clipping telemetry notifications on characteristic UUID `0xFFF2`, and keeps optional framed 16 kHz, 16-bit mono PCM debug notifications on characteristic UUID `0xFFF1`. The Classic Bluetooth HFP path remains quarantined as legacy non-StickS3 code.

## Candidate matrix

| Candidate | StickS3 status | Notes |
| --- | --- | --- |
| Classic Bluetooth HFP | Rejected for StickS3 | ESP32-S3 does not support Bluetooth Classic / BR/EDR. |
| USB Audio device | Unknown / candidate | Must be verified against official ESP-IDF TinyUSB/UAC support and product requirement for wired operation. |
| Bluetooth LE sound-level telemetry | Selected | Uses BLE advertising plus custom GATT notify characteristics for sound-level metrics and optional PCM debug; requires a custom BLE central/host receiver and is not an OS-standard microphone class. |
| BLE custom GATT audio/control | Unknown / candidate | Would require a custom host app and is not a standard OS microphone by itself. |
| BLE Audio | Unknown | Must not be selected until official ESP-IDF documentation confirms exact ESP32-S3 support and required roles. |
| Retarget to Classic-BT-capable ESP32 board | Candidate only if product stops being StickS3 | Would require renaming board docs, constants, and CI target. |

## Local speaker output requirement

Local speaker monitoring is **deferred**. The default BLE GATT PCM firmware uses capture-only I2S and an ES8311 ADC-only profile; it does not enable I2S TX, unmute the DAC, or pulse the speaker amplifier. If the selected transport requires output monitoring, implement verified M5PM1 speaker amplifier control before exposing monitoring as a product feature. If the selected transport is microphone-only, remove or keep disabled monitoring UI actions. Legacy HFP code is historical and is not authoritative for StickS3 audio bring-up.
