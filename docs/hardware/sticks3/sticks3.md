# M5Stack StickS3 hardware reference

This is the main entry point for the StickS3 hardware documentation in this repository. It links the topic-specific hardware notes, the checked-in board manifest, and the source references used to validate board facts. Keep this file, `sticks3.board.json`, the `sticks3_*.md` topic files, and `src/board/board_sticks3.h` synchronized when board facts change.

## Topic files

- [Battery and power facts](sticks3_battery.md)
- [Buttons](sticks3_button.md)
- [Display](sticks3_display.md)
- [BMI270 IMU](sticks3_imu.md)
- [IR NEC](sticks3_ir_nec.md)
- [M5PM1 power controller](sticks3_m5pm1.md)
- [Microphone and capture audio](sticks3_mic.md)
- [Speaker](sticks3_speaker.md)
- [Wakeup](sticks3_wakeup.md)
- [Shared board facts, pin map, automation scope, and ADC facts](sticks3_others.md)
- [Machine-readable board manifest](sticks3.board.json)

## Sources

- M5Stack StickS3 documentation and pin map: https://docs.m5stack.com/en/core/StickS3
- M5Stack StickS3 Arduino programming documentation: https://docs.m5stack.com/en/arduino/m5sticks3/program
- M5Stack StickS3 Battery Arduino documentation: https://docs.m5stack.com/en/arduino/m5sticks3/battery
- M5Stack StickS3 Button Arduino documentation: https://docs.m5stack.com/en/arduino/m5sticks3/button
- M5Stack StickS3 Display Arduino documentation: https://docs.m5stack.com/en/arduino/m5sticks3/display
- M5Stack StickS3 IMU Arduino documentation: https://docs.m5stack.com/en/arduino/m5sticks3/imu
- M5Stack StickS3 IR NEC Arduino documentation: https://docs.m5stack.com/en/arduino/m5sticks3/ir_nec
- M5Stack StickS3 Microphone Arduino documentation: https://docs.m5stack.com/en/arduino/m5sticks3/mic
- M5Stack StickS3 Speaker Arduino documentation: https://docs.m5stack.com/en/arduino/m5sticks3/speaker
- M5Stack StickS3 Wakeup Arduino documentation: https://docs.m5stack.com/en/arduino/m5sticks3/wakeup
- M5Stack StickS3 M5PM1 Arduino documentation: https://docs.m5stack.com/en/arduino/m5sticks3/m5pm1
- M5Stack M5PM1 source repository: https://github.com/m5stack/M5PM1
- M5Stack M5Unified source repository: https://github.com/m5stack/M5Unified
- M5Stack M5GFX source repository: https://github.com/m5stack/M5GFX
- M5Stack M5GFX StickS3 initialization source: https://github.com/m5stack/M5GFX/blob/master/src/M5GFX.cpp
- StickS3 schematic PDF: https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150_Stick_S3_PRJ_V0.6_20251111_2025_11_17_16_10_24.pdf
- ES8311 datasheet: https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/products/atom/Atomic%20Echo%20Base/ES8311.pdf
- BMI270 datasheet: https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/core/K128%20CoreS3/BMI270.PDF
- ESP32-S3 technical reference manual: https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/477/esp32-s3_technical_reference_manual_cn.pdf
- Espressif Bluetooth architecture documentation: https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/bt-architecture/overview.html

## In-repo implementation references

- Firmware board constants: ../../../src/board/board_sticks3.h
- Shared I2C and PMIC helpers: ../../../src/board/board_i2c.c, ../../../src/board/m5pm1.c
- Audio board support: ../../../src/audio/board_audio.c, ../../../src/audio/board_audio_clock.c, ../../../src/audio/board_audio_power.c
- Hardware fact service: ../../../src/board/hardware_fact_service.c
- Rule capability validation: ../../../src/rules/rule_types.c
