# Rule automation firmware smoke checklist

This checklist closes the Phase 14 rule-automation validation task. It records what must be verified on StickS3 hardware and which items were unavailable in this environment.

## Environment status

- **Host environment used by this agent:** no attached StickS3 hardware and no ESP-IDF `idf.py` command available.
- **Hardware-only items:** marked unavailable here and must be re-run on a physical StickS3 before claiming hardware validation.
- **Software checks available here:** host tests and documentation consistency checks.

## Required smoke items

| Item | Status in this environment | Hardware validation steps |
| --- | --- | --- |
| Boot | Unavailable: no StickS3 attached | Flash the ESP-IDF build, power-cycle StickS3, and confirm the app reaches the normal sound-meter UI without error state. |
| Sound telemetry | Unavailable: no StickS3/BLE hardware attached | Subscribe to BLE sound telemetry and confirm RMS/peak/clipping packets change under sound input. |
| Web page | Unavailable: no networked StickS3 attached | Connect to the firmware network path, open `/`, and verify the rule setup page and `/api/status` respond. |
| Config save/reload | Unavailable: no NVS-backed hardware run | Save a sound or button rule through `POST /api/config`, reboot, then confirm `GET /api/config` returns the saved rule with secrets masked. |
| BLE rule event | Unavailable: no BLE central/hardware attached | Enable the BLE rule-event notification, fire a configured rule, and confirm the rule-event packet is received independently of sound telemetry. |
| HTTP POST with test endpoint | Unavailable: no Wi-Fi/network target attached | Configure an HTTP POST action to a local test endpoint, mark network ready through the firmware network path, fire `/api/rules/test`, and confirm the endpoint receives one bounded JSON event. |
| IR test command | Unavailable: no IR receiver/hardware attached | Configure a NEC IR action, trigger `/api/rules/test`, and confirm a matching NEC frame on an IR receiver or logic analyzer. |
| GPIO digital trigger | Unavailable: no external GPIO fixture attached | Configure a safe GPIO digital rule on a validated pin, toggle the input after debounce, and confirm one normalized `gpio.digital.<pin>` fact fires the configured action. |
| Supported HAT probe | Unavailable: no supported HAT attached and HAT capabilities are currently disabled until drivers are verified | Attach only a HAT with an implemented capability; confirm `/api/hat/probe` reports it. Current firmware should fail closed for unsupported HATs. |

## Checks completed in this environment

- Host tests cover config validation, comparator behavior, transition firing, sustain duration, cooldown, unsupported HAT rejection, unsafe GPIO rejection, GPIO digital debounce, web config save/import/export, and action modules.
- Documentation consistency and legacy-keyword checks were run as part of the implementing change.
- ESP-IDF firmware build is still required in an ESP-IDF environment because `idf.py` is unavailable here.
