# BLE sound-meter and rule-event protocol

The default StickS3 firmware advertises as `M5StickS3-Meter` and exposes a custom Bluetooth LE GATT service. This protocol is for custom host applications and validation tools; it is not Bluetooth Classic HFP, BLE Audio, USB Audio, or an operating-system-native microphone profile.

## Service and characteristics

| UUID | Name | Properties | Current behavior |
| --- | --- | --- | --- |
| `0xFFF0` | Service | Primary service | Custom StickS3 sound-meter service. |
| `0xFFF1` | Raw PCM debug | Read, notify | Optional framed `M5S3` 16 kHz, 16-bit mono PCM. Disabled unless PCM debug mode is enabled. |
| `0xFFF2` | Sound level metrics | Read, notify | Compact `M5LM` RMS/peak/VU/clipping telemetry. |
| `0xFFF3` | Control | Write | One-byte app/display/control commands. |
| `0xFFF4` | Status | Read, notify | `M5TS` runtime status packet. |
| `0xFFF5` | Rule events | Notify | `M5RE` automation event packet when a BLE-message action fires. |

## Raw PCM debug frame (`0xFFF1`)

The raw PCM debug characteristic uses a small frame header followed by PCM bytes. The stream is diagnostic-only and is disabled unless `CONFIG_APP_SOUND_METER_ENABLE_PCM_DEBUG=y` and debug mode is enabled at runtime.

| Field | Type | Meaning |
| --- | --- | --- |
| magic | `uint32_t` | Little-endian `M5S3`. |
| version | `uint16_t` | Protocol version. |
| packet bytes | `uint16_t` | Total bytes in the notification. |
| sequence | `uint32_t` | Incrementing packet sequence. |
| uptime ms | `uint32_t` | Device uptime when the frame is sent. |
| sample rate Hz | `uint32_t` | Current PCM sample rate, normally 16000. |
| flags | `uint16_t` | Runtime/debug flags. |
| PCM payload | bytes | 16-bit mono PCM payload. |

## Sound-level packet (`0xFFF2`, magic `M5LM`)

| Offset | Type | Field |
| ---: | --- | --- |
| 0 | `uint32_t` | magic |
| 4 | `uint16_t` | version |
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

## Status packet (`0xFFF4`, magic `M5TS`)

| Offset | Type | Field |
| ---: | --- | --- |
| 0 | `uint32_t` | magic |
| 4 | `uint16_t` | version |
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
| 28 | `uint32_t` | completed metric windows |
| 32 | `uint32_t` | I2S read errors |

## Rule-event packet (`0xFFF5`, magic `M5RE`)

| Offset | Type | Field |
| ---: | --- | --- |
| 0 | `uint32_t` | magic |
| 4 | `uint16_t` | version |
| 6 | `uint16_t` | packet bytes |
| 8 | `uint32_t` | rule event sequence |
| 12 | `uint32_t` | uptime ms |
| 16 | `uint32_t` | rule id |
| 20 | `uint16_t` | source enum value |
| 22 | `uint16_t` | action enum value |
| 24 | `int32_t` | measured value as signed integer or 0/1 boolean |
| 28 | `uint32_t` | rule fire count |
| 32 | `char[20]` | truncated rule name |

## Control commands (`0xFFF3`)

| Byte | Command |
| ---: | --- |
| `0x01` | Cycle app mode |
| `0x02` | Cycle display mode |
| `0x03` | Enable PCM debug mode, if firmware config allows it |
| `0x04` | Disable PCM debug and return to sound-meter mode |
| `0x05` | Enter calibration mode |
| `0x06` | Pause sound-meter monitoring |
| `0x07` | Resume sound-meter mode |

## App modes

| Value | Mode |
| ---: | --- |
| 0 | Sound meter |
| 1 | PCM debug stream |
| 2 | Calibration |
| 3 | Paused |

## Display modes

| Value | Mode |
| ---: | --- |
| 0 | VU meter |
| 1 | Numeric metrics |
| 2 | BLE/status |
| 3 | Diagnostics |
