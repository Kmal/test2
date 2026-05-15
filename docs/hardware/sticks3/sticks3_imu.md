# StickS3 BMI270 IMU hardware notes

BMI270 is documented on the shared I2C bus at `0x68`, with interrupt routing through M5PM1. The current firmware uses BMI270 only for polling-based `bmi270.motion` facts when `CONFIG_APP_BMI270_FACTS=y`; it does not enable BMI270 interrupt mode and does not modify the M5PM1 IMU interrupt GPIO. M5PM1 GPIO helpers must preserve unrelated GPIO fields so future IMU interrupt routing is not clobbered by audio-related changes.

## BMI270 motion: `bmi270.motion`

* Source: BMI270 accelerometer polling at I2C address `0x68`.
* First implementation: polling-only software thresholding using `CONFIG_APP_BMI270_MOTION_DELTA_MG` and a fixed still hysteresis of `30 mg`; initialization uses the BMI270 normal-power polling register path (`PWR_CONF=0x02`, accelerometer 100 Hz normal bandwidth, ±2g range, `PWR_CTRL.acc_en`).
* Interrupt deferral: the implementation does not enable BMI270 interrupt mode and does not modify M5PM1 GPIO4 / `PYG4_IMU_INT`.
* Future interrupt mode must preserve existing M5PM1 GPIO state and avoid disturbing M5PM1 GPIO2 / `PYG2_L3B_EN` LCD/audio power behavior.
* Configuration: `CONFIG_APP_BMI270_FACTS=y` is set in `config/sdkconfig.defaults`; emitted only while that Kconfig option remains enabled.

## Sources

- M5Stack StickS3 product page pin map for BMI270 address `0x68` and M5PM1 `PYG4_IMU_INT`: https://docs.m5stack.com/en/core/StickS3
- M5Stack StickS3 IMU Arduino API page for the official IMU feature: https://docs.m5stack.com/en/arduino/m5sticks3/imu
- BMI270 datasheet used for polling-register semantics: https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/core/K128%20CoreS3/BMI270.PDF
- Firmware BMI270 polling implementation: ../../../src/board/bmi270.c
- Firmware hardware fact implementation for `bmi270.motion`: ../../../src/board/hardware_fact_service.c
