# Project documentation

These documents describe the current StickS3 firmware as a custom BLE sound-meter and local automation device. Do not describe it as a Classic Bluetooth HFP microphone or as an operating-system-native microphone.

- `../README.md` is the top-level status document: current firmware behavior, completed work, next plan, pin map, and checks.
- `hardware/sticks3.md` records verified StickS3 hardware facts, shared I2C ownership, audio clocking, LCD/button mapping, M5PM1 L3B power behavior, and blocked hardware features.
- `transport-feasibility.md` explains the selected custom BLE sound-meter transport and rejected/deferred alternatives.
- `ble-sound-meter-protocol.md` documents the custom BLE service, packet formats, control commands, status packets, and rule-event notifications.
- `rule-automation-firmware-plan.md` records which automation phases are complete, what the firmware now implements, and what remains planned.
- `rule-automation-smoke-checklist.md` lists hardware smoke checks that must be run before claiming end-to-end physical validation.
- `acceptance-tests.md` defines static, host, ESP-IDF, hardware, sound-meter, and automation acceptance checks.
- `change-checklist.md` lists required documentation, tests, and no-invention rules for future hardware, transport, and automation changes.

Factory-style release images are created with `python3 tools/make_factory_image.py` after `idf.py build`; the merged image contains every binary listed by ESP-IDF in `build/flash_args` and is the artifact intended for offset `0x0` flashing.
