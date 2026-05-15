# StickS3 battery and power-fact hardware notes

The checked-in `config/sdkconfig.defaults` profile enables the StickS3 battery and USB/external-power fact Kconfig gates, so these facts are compiled into the standard firmware unless a menuconfig opt-out build disables them. The implementation is source-aligned with the official StickS3 pin map, Arduino examples, M5PM1 guidance, and ESP32-S3 ADC capability, but it is **not** a full product conformance claim until the facts are validated on real StickS3 hardware.

## Official-source conformance boundary

| Fact area | Official source point | Firmware boundary |
| --- | --- | --- |
| Battery percent | The StickS3 battery example exposes charging status, battery level, and battery voltage through `M5.Power`. | Firmware reads the M5PM1 VBAT millivolt registers directly and converts valid readings through its own LiPo interpolation curve; this is source-backed, but the exact percentage curve still needs physical comparison against M5Unified on StickS3. |
| USB/external power | The product page documents USB Type-C 5 V input and warns that EXT_5V/Grove/Hat2-Bus 5 V can be input or output depending on EXT_5V mode. | Firmware reads M5PM1 VIN and 5V voltage registers independently of VBAT availability and compares them with `CONFIG_APP_POWER_USB_PRESENT_MV`; it reports USB presence when either readable rail crosses the threshold, reports absence only when both rails read below the threshold, and does not switch EXT_5V output/input mode or infer current direction. |

## Battery percent: `power.battery_percent`

* Source: M5PM1 VBAT millivolt registers on the shared I2C bus (`SDA=GPIO47`, `SCL=GPIO48`, PMIC address `0x6e`).
* Policy: VBAT samples are considered valid only in the `2500..4500 mV` range.
* Conversion: valid VBAT values are converted through the LiPo interpolation curve used by `board_power_lipo_percent_from_mv()`.
* Configuration: `CONFIG_APP_BATTERY_FACTS=y` is set in `config/sdkconfig.defaults`; emitted only while that Kconfig option remains enabled.

## USB/external power present: `power.usb_present`

* Source: M5PM1 VIN and 5V input/output millivolt reads.
* Policy: USB/external power is present when either VIN or 5V in/out is at least `CONFIG_APP_POWER_USB_PRESENT_MV`.
* The StickS3 documentation warns that the 5V interface can be input or output depending on EXT_5V mode; automation only reads voltage and does not change EXT_5V/L3B policy.
* Configuration: `CONFIG_APP_USB_POWER_FACTS=y` is set in `config/sdkconfig.defaults`; emitted only while that Kconfig option remains enabled.

## Sources

- M5Stack StickS3 product page for USB Type-C input, EXT_5V/Grove/Hat2-Bus power-mode warning, battery capacity, and `M5.Power` battery APIs: https://docs.m5stack.com/en/core/StickS3
- M5Stack StickS3 battery Arduino API page for charging status, battery level, and battery voltage APIs: https://docs.m5stack.com/en/arduino/m5sticks3/battery
- Firmware configuration defaults for `CONFIG_APP_BATTERY_FACTS`, `CONFIG_APP_USB_POWER_FACTS`, and `CONFIG_APP_POWER_USB_PRESENT_MV`: ../../../config/sdkconfig.defaults
- Firmware PMIC power implementation: ../../../src/board/board_power.c
