# Acceptance tests

The project can claim working StickS3 firmware only when the checks below match the selected transport.

## Static acceptance

- `python3 tools/check_board_map.py`
- `python3 tools/check_transport_config.py`
- `python3 tools/check_docs_consistency.py`
- `python3 tools/check_audio_clock.py`
- `python3 tools/check_audio_safety.py`

## Host acceptance

- `tests/host/run_host_tests.sh`

Host tests cover pure C helpers, audio metrics, application mode cycling, ES8311 register sequencing through a fake register bus, bit-preserving M5PM1 GPIO helpers, board-audio operation ordering, failure cleanup policy, and the documented audio clock profile.

## ESP-IDF acceptance

- Positive `esp32s3` build for the selected StickS3-compatible transport.
- `python3 tools/make_factory_image.py` produces `build/m5sticks3_bluetooth_mic.bin` from the same `build/flash_args` that ESP-IDF uses for flashing, so release/provisioning flows receive a complete merged image for offset `0x0`; the ESP-IDF application-only artifact remains separately named `build/m5sticks3_bluetooth_mic_app.bin`.
- The ESP-IDF GitHub Actions job must create `build/m5sticks3_bluetooth_mic.bin`, write `build/m5sticks3_bluetooth_mic.bin.sha256`, and upload both files so successful workflow runs provide a ready-to-flash factory artifact.
- Legacy Classic Bluetooth HFP must remain blocked for `esp32s3`.
- The default Bluetooth LE sound-meter profile must use capture-only I2S RX and the ES8311 ADC-only profile.
- M5PM1 L3B audio-rail writes must follow the documented/tested GPIO2 sequence: GPIO function, output mode, push-pull drive, and high output at the PMIC's 100 kHz power-up I2C speed; speaker-amplifier pulse/control writes remain blocked unless their source-backed sequence is documented and tested.

## Hardware smoke acceptance

- I2C probe sees ES8311 at `0x18`.
- I2C probe sees M5PM1 at `0x6e`; default capture-only audio requires it only for the source-backed L3B audio rail, while the separate optional identity probe remains disabled.
- I2C probe sees BMI270 at `0x68` if IMU support or optional board-presence probing is in scope.
- GPIO18 MCLK measures 12.288 MHz for the current fixed MCLK profile.
- GPIO17 BCLK measures the documented 512 kHz target for 16 kHz, 16-bit mono capture.
- GPIO15 LRCK/WS measures 16 kHz.
- Default Bluetooth LE GATT PCM boot leaves the I2S TX path, ES8311 DAC, and speaker amplifier disabled.
- Microphone capture produces nonzero PCM under sound input after source-backed audio power/clock/codec setup is complete.
- Selected BLE sound-meter transport advertises as `M5StickS3-Meter`, renders a live LCD VU meter, delivers `M5LM` telemetry notifications to a BLE central subscribed to service UUID `0xFFF0`, characteristic UUID `0xFFF2`, accepts one-byte control writes on characteristic UUID `0xFFF3`, and returns an `M5TS` status packet on characteristic UUID `0xFFF4`; optional raw PCM debug remains on characteristic UUID `0xFFF1` when explicitly enabled.
- Speaker output works only if monitoring is in scope and M5PM1 amplifier control is implemented from source-backed evidence.
- IR receive is tested with speaker amplifier disabled if IR support is in scope.

## Failure-path acceptance

- If shared I2C initialization fails, no M5PM1, I2S, or ES8311 operation should run.
- If optional M5PM1 identity probing is disabled for default capture-only audio, identity-read failure must not block I2S or ES8311 initialization; the required L3B power-enable write path is still fail-fast.
- If a future profile explicitly requires M5PM1 probing and the probe fails, I2S and ES8311 operations should not run.
- If the source-backed audio-power enable step fails, I2S and ES8311 operations should not run.
- If I2S initialization fails, ES8311 initialization should not run.
- Production cleanup currently logs failures and avoids guessed power-disable writes.

## Sound-meter smoke acceptance

- LCD shows the `M5S3 LEVEL` VU page after boot when `CONFIG_APP_STATUS_UI_LCD=y`.
- Quiet input produces a low VU percentage and speaking/clapping near the microphone increases RMS, peak, and VU values.
- KEY1 cycles VU, numeric, BLE status, and diagnostics display pages.
- KEY2 cycles enabled application modes, skipping raw PCM debug when `CONFIG_APP_SOUND_METER_ENABLE_PCM_DEBUG=n`.
- BLE central subscription to `0xFFF2` receives compact sound-level telemetry at roughly `CONFIG_APP_SOUND_METER_TELEMETRY_HZ`.
- BLE central writes to `0xFFF3` can cycle app/display modes, pause/resume the sound meter, and enter calibration mode.
- BLE central reads `0xFFF4` and receives an `M5TS` status packet.
- The firmware does not claim calibrated SPL/dBA accuracy unless calibrated against an external acoustic reference.
