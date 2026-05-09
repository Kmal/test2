# Acceptance tests

The project can claim working StickS3 firmware only when the checks below match the selected transport.

## Static acceptance

- `python3 tools/check_board_map.py`
- `python3 tools/check_transport_config.py`
- `python3 tools/check_docs_consistency.py`

## Host acceptance

- `tests/host/run_host_tests.sh`

## ESP-IDF acceptance

- Positive `esp32s3` build for the selected StickS3-compatible transport.
- Legacy Classic Bluetooth HFP must remain blocked for `esp32s3`.
- ES8311 component or mocked-I2C tests when those are added.

## Hardware smoke acceptance

- I2C probe sees ES8311 at `0x18`.
- I2C probe sees M5PM1 at `0x6e`.
- I2C probe sees BMI270 at `0x68` if IMU support is in scope.
- Microphone capture produces nonzero PCM under sound input.
- Selected transport delivers microphone audio to the host.
- Speaker output works only if monitoring is in scope and M5PM1 amplifier control is implemented.
- IR receive is tested with speaker amplifier disabled if IR support is in scope.
