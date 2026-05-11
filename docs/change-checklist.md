# Hardware, transport, and automation change checklist

For every hardware, transport, web, or automation change, update or explicitly confirm:

- `README.md`
- `docs/hardware/sticks3.md`
- `docs/hardware/sticks3.board.json` when board facts change
- `docs/transport-feasibility.md` when transport behavior changes
- `docs/ble-sound-meter-protocol.md` when BLE packets, UUIDs, commands, or semantics change
- `README.md` automation status/roadmap sections when rule capabilities, endpoints, or roadmap status change
- `README.md` hardware smoke checklist and `docs/acceptance-tests.md` when validation steps change
- Source comments in touched code
- `config/sdkconfig.defaults` when default config behavior changes
- `main/Kconfig.projbuild` when options change
- CMake files when source layout changes
- Static validation scripts and host tests
- Factory-image generation flow and `tools/make_factory_image.py` when build or flash artifacts change

## No-invention hardware write policy

Every new hardware write sequence must document or cite:

- source document or source-code link;
- device address;
- register address;
- bit mask;
- intended value;
- reset/default behavior if known;
- whether the register must be updated with read-modify-write;
- unrelated fields that must be preserved;
- host tests proving bit preservation for shared registers.

If any required hardware behavior is unknown, keep the behavior blocked or feature-gated. Do not guess M5PM1 L3B power polarity, M5PM1 speaker amplifier pulse sequences, ES8311 volatile register readback behavior, BMI270 interrupt routing, HAT driver protocols, ADC paths, or safe external GPIO routes.

## Documentation truthfulness policy

- Describe the current product as a custom BLE sound-meter and local automation device.
- Do not call the firmware a Classic Bluetooth HFP microphone or OS-native microphone.
- State whether a capability is implemented, debug-only, planned, or deliberately disabled.
- If docs and code disagree, fix both or document the exact limitation before merging.
