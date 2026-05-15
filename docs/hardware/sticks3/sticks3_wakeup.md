# StickS3 wakeup hardware notes

The M5Stack wakeup documentation routes BMI270 INT1 through M5PM1 GPIO4/PYG4, and this repo treats that as a deferred hardware path. The current firmware does not enable BMI270 interrupt mode, does not configure M5PM1 wake routing, and does not modify M5PM1 GPIO4 / `PYG4_IMU_INT`.

Future wakeup work must preserve existing M5PM1 GPIO state and avoid disturbing GPIO2 / `PYG2_L3B_EN` LCD/audio power behavior or GPIO3 / `PYG3_SPK_Pulse` speaker-amplifier behavior.

## Sources

- M5Stack StickS3 product page pin map for M5PM1 `PYG4_IMU_INT`, `PYG2_L3B_EN`, and `PYG3_SPK_Pulse`: https://docs.m5stack.com/en/core/StickS3
- M5Stack StickS3 wakeup Arduino API page for the official wakeup feature: https://docs.m5stack.com/en/arduino/m5sticks3/wakeup
- Firmware M5PM1 GPIO helpers that must preserve unrelated GPIO state: ../../../src/board/m5pm1.c
- Firmware BMI270 implementation showing current polling-only behavior: ../../../src/board/bmi270.c
