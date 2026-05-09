# Project documentation

- `hardware/sticks3.md` records verified StickS3 hardware facts, source-gated unknowns, shared I2C ownership, the audio clock profile, the selected BLE GATT PCM transport, and deferred hardware decisions.
- `transport-feasibility.md` records the current transport decision state.
- `acceptance-tests.md` defines static, host, build, and hardware acceptance checks.
- `change-checklist.md` lists required updates and no-invention rules for hardware and transport changes.
- Factory-style release images are created with `python3 tools/make_factory_image.py` after `idf.py build`; the merged image contains every binary listed by ESP-IDF in `build/flash_args`.
