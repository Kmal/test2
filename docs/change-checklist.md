# Hardware / transport change checklist

For every hardware or transport change, update or explicitly confirm:

- `README.md`
- `docs/hardware/sticks3.md`
- `docs/hardware/sticks3.board.json`
- Source comments in touched code
- `config/sdkconfig.defaults` when config behavior changes
- `main/Kconfig.projbuild` when options change
- CMake files when source layout changes
- CI workflow when commands change
- Static validation scripts and tests
- `tests/host/run_host_tests.sh` when host tests/fakes change

## No-invention hardware write policy

Every new hardware write sequence must document or cite:

- source document or source-code link,
- device address,
- register address,
- bit mask,
- intended value,
- reset/default behavior if known,
- whether the register must be updated with read-modify-write,
- unrelated fields that must be preserved,
- host tests proving bit preservation for shared registers.

If any required hardware behavior is unknown, keep the behavior blocked or feature-gated. Do not guess M5PM1 L3B power polarity, M5PM1 speaker amplifier pulse sequences, ES8311 volatile register readback behavior, or BMI270 interrupt routing.
