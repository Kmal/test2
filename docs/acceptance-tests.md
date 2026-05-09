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

Host tests cover pure C helpers, ES8311 register sequencing through a fake register bus, bit-preserving M5PM1 GPIO helpers, board-audio operation ordering, failure cleanup policy, and the documented audio clock profile.

## ESP-IDF acceptance

- Positive `esp32s3` build for the selected StickS3-compatible transport.
- Legacy Classic Bluetooth HFP must remain blocked for `esp32s3`.
- The default Bluetooth LE GATT PCM profile must use capture-only I2S RX and the ES8311 ADC-only profile.
- M5PM1 L3B and speaker-control writes must remain blocked unless their source-backed polarity/sequence is documented and tested.

## Hardware smoke acceptance

- I2C probe sees ES8311 at `0x18`.
- I2C probe sees M5PM1 at `0x6e`.
- I2C probe sees BMI270 at `0x68` if IMU support or optional board-presence probing is in scope.
- GPIO18 MCLK measures 12.288 MHz for the current fixed MCLK profile.
- GPIO17 BCLK measures the documented 512 kHz target for 16 kHz, 16-bit mono capture.
- GPIO15 LRCK/WS measures 16 kHz.
- Default Bluetooth LE GATT PCM boot leaves the I2S TX path, ES8311 DAC, and speaker amplifier disabled.
- Microphone capture produces nonzero PCM under sound input after source-backed audio power/clock/codec setup is complete.
- Selected BLE GATT PCM transport delivers framed microphone notifications to a BLE central subscribed to service UUID `0xFFF0`, characteristic UUID `0xFFF1`.
- Speaker output works only if monitoring is in scope and M5PM1 amplifier control is implemented from source-backed evidence.
- IR receive is tested with speaker amplifier disabled if IR support is in scope.

## Failure-path acceptance

- If shared I2C initialization fails, no M5PM1, I2S, or ES8311 operation should run.
- If M5PM1 probe fails, I2S and ES8311 operations should not run.
- If a source-backed audio-power enable step is later required and fails, I2S and ES8311 operations should not run.
- If I2S initialization fails, ES8311 initialization should not run.
- Production cleanup currently logs failures and avoids guessed power-disable writes.
