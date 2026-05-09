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
