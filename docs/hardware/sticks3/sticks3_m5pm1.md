# StickS3 M5PM1 and shared power-controller notes

## Shared I2C bus

ES8311 (`0x18`), BMI270 (`0x68`), and M5PM1 (`0x6e`) share the GPIO47/GPIO48 I2C bus. `src/board/board_i2c.*` owns bus installation. Device drivers must not install the I2C driver independently.

## M5PM1 L3B audio/LCD power sequence

The StickS3 documentation and schematic identify M5PM1/PY `G2` as `PYG2_L3B_EN`, with the ES8311 audio rail on `3V3_L3B_AU`. The implemented helper configures M5PM1 GPIO2 as a normal output, push-pull drive, and **high** output using the same tested GPIO-helper path as LCD/L3B setup. The official M5PM1 StickS3 guidance enables L3B with `gpioSetOutput(..., true)`, after which this firmware writes `I2C_CFG=0x00` and waits for the rail before LCD reset. M5PM1 powers up in 100 kHz I2C mode, so the shared register-bus cache keeps the M5PM1 device handle at 100 kHz while other devices can use the board-default 400 kHz speed. The PMIC helper retries a first `ESP_ERR_INVALID_RESPONSE` once, because the boot log showed the LCD path failing on the first M5PM1 GPIO-function read while a later L3B access succeeded. The separate M5PM1 identity probe remains optional. In the default boot, this sequence is reached through LCD power initialization when LCD is enabled and through demand-driven sound-level audio initialization when enabled `sound.*` rules or Web UI telemetry demand exist.

This L3B enable sequence is separate from speaker amplification. The Kconfig-gated `speaker_tone` action uses the source-backed M5PM1 PYG3 SPK amplifier sequence (`GPIO3` function, output mode, push-pull drive, high to enable and low to disable) and releases playback audio resources after each tone so demand-driven microphone capture can restart.

## Hardware fact PMIC use

M5PM1 backs the `power.battery_percent` and `power.usb_present` facts. M5PM1 voltage registers are read as little-endian 16-bit millivolt values so USB/VIN values above 4095 mV remain representable. The firmware reads voltage only; it does not switch EXT_5V output/input mode and it does not guess unsupported M5PM1 charge-state bits.

## Sources

- M5Stack StickS3 product page pin map for M5PM1 address `0x6e`, `PYG2_L3B_EN`, `PYG3_SPK_Pulse`, and shared I2C pins: https://docs.m5stack.com/en/core/StickS3
- M5Stack StickS3 M5PM1 Arduino API page for official M5PM1 operations: https://docs.m5stack.com/en/arduino/m5sticks3/m5pm1
- M5PM1 source repository for PMIC register/API behavior: https://github.com/m5stack/M5PM1
- M5GFX StickS3 initialization source for the vendor L3B/LCD power-up sequence: https://github.com/m5stack/M5GFX/blob/master/src/M5GFX.cpp
- Firmware M5PM1 GPIO and L3B/LCD power implementation: ../../../src/board/m5pm1.c
- Firmware PMIC power-fact implementation: ../../../src/board/board_power.c
