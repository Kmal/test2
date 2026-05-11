# Rule automation firmware smoke checklist

This checklist records what must be verified on physical StickS3 hardware for the current local automation firmware. Host tests can prove logic behavior, but they cannot prove board boot, RF, GPIO fixtures, IR signaling, or audio clocks.

## Environment status

- Host-only environments can run static checks and `tests/host/run_host_tests.sh`.
- ESP-IDF build checks require an ESP-IDF environment.
- Hardware-only items below require an attached StickS3 and any listed external fixture.

## Required smoke items

| Item | Hardware validation steps |
| --- | --- |
| Boot | Flash the merged ESP-IDF image or use `idf.py flash`, power-cycle StickS3, and confirm the app reaches the sound-meter UI without error state. |
| Sound telemetry | Subscribe to BLE service `0xFFF0`, characteristic `0xFFF2`, and confirm `M5LM` RMS/peak/VU/clipping packets change under sound input. |
| Status/control | Read/subscribe to `0xFFF4`, write commands to `0xFFF3`, and confirm app/display modes update on the LCD. |
| Rule event | Configure a BLE-message rule, subscribe to `0xFFF5`, fire the rule, and confirm an `M5RE` event packet. |
| Wi-Fi setup | Verify saved station credentials, setup AP fallback, LCD keyboard provisioning when enabled, and `/api/wifi/status`. |
| Web page | Open `/` from the reachable network path and verify status, capabilities, config export/import, and Wi-Fi controls respond. |
| Config save/reload | Save a sound or button rule through `POST /api/config`, reboot, then confirm `GET /api/config` returns the saved rule with secrets masked. |
| HTTP POST action | Configure an HTTP POST action to a local test endpoint, fire `/api/rules/test`, and confirm one bounded JSON event arrives. |
| IR send action | Configure a NEC IR action, trigger `/api/rules/test`, and confirm a matching NEC frame on an IR receiver or logic analyzer. |
| GPIO digital/edge trigger | Configure a safe GPIO rule on a validated pin, toggle the input after debounce, and confirm exactly one normalized GPIO fact fires the configured action. |
| Fail-closed HAT probe | Call `/api/hat/probe` for unsupported HAT sources and confirm the response remains unsupported until a real HAT driver is implemented. |
| Audio clocks | Measure GPIO18 MCLK at 12.288 MHz, GPIO17 BCLK at the documented 512 kHz target, and GPIO15 LRCK at 16 kHz. |
| Capture-only safety | Confirm the default boot does not drive I2S TX, unmute the ES8311 DAC, or pulse the speaker amplifier. |

## Host checks expected before hardware smoke

- Static validation scripts pass.
- Host tests pass for config validation, comparator behavior, transition firing, sustain duration, cooldown, unsupported HAT rejection, unsafe GPIO rejection, GPIO digital debounce, web config save/import/export, and action modules.
- ESP-IDF build succeeds for `esp32s3` before flashing hardware.
