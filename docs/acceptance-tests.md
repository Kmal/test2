# Acceptance tests

The project can claim working StickS3 firmware only when the checks below match the current custom BLE sound-meter and local automation product. Do not use these checks to claim Classic Bluetooth HFP, BLE Audio, USB Audio, or OS-native microphone behavior.

## Static acceptance

- `python3 tools/check_board_map.py`
- `python3 tools/check_transport_config.py`
- `python3 tools/check_docs_consistency.py`
- `python3 tools/check_audio_clock.py`
- `python3 tools/check_audio_safety.py`

## Host acceptance

- `tests/host/run_host_tests.sh`

Host tests cover pure C helpers, audio metrics, app mode cycling, ES8311 sequencing through a fake register bus, bit-preserving M5PM1 GPIO helpers, board-audio ordering, failure cleanup policy, audio clock profile, BLE protocol helpers, rule validation, rule engine transitions/sustain/cooldown, config storage, trigger adapters, GPIO safety, web handlers, and action modules.

## ESP-IDF acceptance

- Positive `esp32s3` build for the selected BLE sound-meter/automation transport.
- Legacy Classic Bluetooth HFP remains blocked for `esp32s3`.
- The default profile uses capture-only I2S RX and the ES8311 ADC-only path.
- M5PM1 L3B audio-rail writes follow the documented GPIO2 sequence: GPIO function, output mode, push-pull drive, and high output at the PMIC's 100 kHz power-up I2C speed.
- Speaker-amplifier pulse/control writes remain blocked unless their source-backed sequence is documented and tested.
- `python3 tools/make_factory_image.py` produces `build/m5sticks3_bluetooth_mic.bin` from `build/flash_args`; the application-only artifact remains `build/m5sticks3_bluetooth_mic_app.bin`.

## Hardware smoke acceptance

- I2C probe sees ES8311 at `0x18`.
- I2C probe sees M5PM1 at `0x6e`; default capture-only audio requires it for the source-backed L3B audio rail.
- I2C probe sees BMI270 at `0x68` if IMU support or optional board-presence probing is in scope.
- GPIO18 MCLK measures 12.288 MHz for the current fixed MCLK profile.
- GPIO17 BCLK measures the documented 512 kHz target for 16 kHz, 16-bit mono capture.
- GPIO15 LRCK/WS measures 16 kHz.
- Default boot leaves the I2S TX path, ES8311 DAC, and speaker amplifier disabled.
- Microphone capture produces nonzero PCM under sound input after audio power/clock/codec setup is complete.
- BLE advertises as `M5StickS3-Meter` and exposes service `0xFFF0`.
- A BLE central subscribed to `0xFFF2` receives live `M5LM` sound telemetry.
- A BLE central can read/subscribe to `0xFFF4` and receive `M5TS` status.
- One-byte writes to `0xFFF3` change app/display/control state as documented.
- A BLE-message automation action emits an `M5RE` packet on `0xFFF5`.
- Optional raw PCM debug remains on `0xFFF1` only when explicitly enabled.
- The LCD shows the `M5S3 LEVEL` VU page after boot when `CONFIG_APP_STATUS_UI_LCD=y`.
- KEY1 cycles VU, numeric, BLE status, and diagnostics display pages.
- KEY2 cycles enabled application modes, skipping raw PCM debug when `CONFIG_APP_SOUND_METER_ENABLE_PCM_DEBUG=n`.

## Automation acceptance

- `GET /api/capabilities` lists supported sound, button, BLE, Wi-Fi, GPIO digital/edge sources and BLE, HTTP, IR, local UI actions.
- Unsupported HAT, BMI270, battery/power, ADC, GPIO pulse/frequency, and HAT action capabilities are reported disabled and rejected by validation.
- `GET /api/config` exports the running config with secrets masked.
- `POST /api/config` accepts valid bounded configs, saves them to NVS, and replaces the running runtime config.
- Invalid configs return a validation error and do not replace the running config.
- `POST /api/gpio/test` rejects pins that conflict with LCD, I2C, I2S/audio, StickS3 buttons, IR, boot/USB, or internal-risk functions.
- Sound, button, BLE state, Wi-Fi state, and safe GPIO facts can fire configured rules.
- Cooldown and sustain semantics match the host tests.
- HTTP POST actions send one bounded JSON event only when network is ready.
- NEC IR actions produce the configured frame on hardware.

## Failure-path acceptance

- If shared I2C initialization fails, no M5PM1, I2S, or ES8311 operation runs.
- If optional M5PM1 identity probing is disabled for default capture-only audio, identity-read failure does not block I2S or ES8311 initialization; the required L3B power-enable write path is still fail-fast.
- If a future profile explicitly requires M5PM1 probing and the probe fails, I2S and ES8311 operations do not run.
- If the source-backed audio-power enable step fails, I2S and ES8311 operations do not run.
- If I2S initialization fails, ES8311 initialization does not run.
- Production cleanup logs failures and avoids guessed power-disable writes.
- If audio, BLE, or sound-meter startup fails at runtime, the app enters error state and stays alive for diagnostics instead of reset-looping.
