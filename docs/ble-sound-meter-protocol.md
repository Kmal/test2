# BLE sound-meter protocol

The default StickS3 firmware advertises as `M5StickS3-Meter` and exposes a custom Bluetooth LE GATT service. This is not a Bluetooth Classic HFP, BLE Audio, USB Audio, or operating-system-native microphone profile.

## Service and characteristics

| UUID | Name | Properties | Purpose |
| --- | --- | --- | --- |
| `0xFFF0` | Sound-meter service | Primary service | Custom StickS3 sound-meter service. |
| `0xFFF1` | Raw PCM debug | Read, notify | Optional debug stream with `M5S3` framed 16 kHz, 16-bit mono PCM. Disabled unless PCM debug mode is enabled. |
| `0xFFF2` | Sound-level telemetry | Read, notify | Main `M5LM` metrics stream with RMS, peak, VU, clipping, sequence, and mode fields. |
| `0xFFF3` | Control | Write | One-byte command interface for mode/display control. |
| `0xFFF4` | Status | Read, notify | `M5TS` status snapshot with runtime mode, notify state, sample rate, window size, and error counters. |

## Sound-level telemetry packet (`0xFFF2`)

All integer fields are little-endian. `dbfs_q8` values encode dBFS multiplied by 256, so `-32.5 dBFS` is `-8320`.

| Offset | Type | Field |
| ---: | --- | --- |
| 0 | `uint32_t` | magic `0x4d4c354d` (`M5LM`) |
| 4 | `uint16_t` | version `1` |
| 6 | `uint16_t` | packet bytes, currently `44` |
| 8 | `uint32_t` | sequence |
| 12 | `uint32_t` | uptime ms |
| 16 | `uint32_t` | sample rate Hz |
| 20 | `uint16_t` | analysis window ms |
| 22 | `uint16_t` | flags |
| 24 | `int32_t` | RMS dBFS Q8 |
| 28 | `int32_t` | peak dBFS Q8 |
| 32 | `uint16_t` | RMS percent |
| 34 | `uint16_t` | peak percent |
| 36 | `uint16_t` | VU percent |
| 38 | `uint16_t` | clipped sample count |
| 40 | `uint8_t` | app mode |
| 41 | `uint8_t` | display mode |
| 42 | `uint8_t` | reserved |
| 43 | `uint8_t` | reserved |

Flags:

| Bit | Meaning |
| ---: | --- |
| 0 | clipping detected |
| 1 | voice-like heuristic |
| 2 | loud threshold reached |
| 3 | underrun reserved |

## Status packet (`0xFFF4`)

| Offset | Type | Field |
| ---: | --- | --- |
| 0 | `uint32_t` | magic `0x5354354d` (`M5TS`) |
| 4 | `uint16_t` | version `1` |
| 6 | `uint16_t` | packet bytes, currently `36` |
| 8 | `uint32_t` | uptime ms |
| 12 | `uint8_t` | app mode |
| 13 | `uint8_t` | display mode |
| 14 | `uint8_t` | BLE connected |
| 15 | `uint8_t` | metrics notify enabled |
| 16 | `uint8_t` | PCM notify enabled |
| 17 | `uint8_t` | PCM debug enabled |
| 18 | `uint8_t` | sound meter enabled |
| 19 | `uint8_t` | reserved |
| 20 | `uint32_t` | sample rate Hz |
| 24 | `uint32_t` | metrics window ms |
| 28 | `uint32_t` | completed windows |
| 32 | `uint32_t` | I2S read errors |

## Control commands (`0xFFF3`)

Write exactly one byte.

| Byte | Command |
| ---: | --- |
| `0x01` | Cycle application mode |
| `0x02` | Cycle display mode |
| `0x03` | Enable PCM debug mode, if firmware config allows it |
| `0x04` | Disable PCM debug and return to sound-meter mode |
| `0x05` | Enter calibration mode |
| `0x06` | Pause sound-meter updates |
| `0x07` | Resume sound-meter mode |

## Application modes

| Value | Mode |
| ---: | --- |
| 0 | Sound meter |
| 1 | PCM debug stream |
| 2 | Calibration |
| 3 | Paused |

Calibration estimates a dBFS noise floor from recent analysis windows. It is not an SPL/dBA calibration and should not be treated as a certified sound-pressure measurement.
