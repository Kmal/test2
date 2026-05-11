# StickS3 local rule automation firmware plan

This plan defines a phased implementation path for StickS3 local rule automation. The firmware will provide a local HTTP rule setup page, normalize onboard, M5Stack HAT, and safe GPIO trigger sources into rule facts, and execute validated DO actions through asynchronous adapters for BLE, HTTP, HAT operations, local UI, and IR.

Implementation identifiers, filenames, module names, public APIs, future C identifiers, logs, and user-facing titles must use project-owned rule automation terminology. Do not introduce external-service-style branding or legacy keyword-based names. Prefer names such as `rule_automation`, `rule_engine`, `trigger_source`, `action_adapter`, `WHEN`, and `DO`.

## Terminology

- **Rule automation**: local firmware feature that evaluates configured WHEN triggers and schedules configured DO actions.
- **WHEN trigger**: the condition side of an automation rule.
- **DO action**: the effect side of an automation rule.
- **Trigger source**: a normalized source of facts, such as sound RMS, KEY1, PIR motion, ENV III humidity, or safe GPIO input.
- **Action adapter**: bounded implementation that performs a DO action outside the rule engine.
- **Rule engine**: deterministic evaluator that turns facts and validated rule configuration into action events without touching hardware or transports directly.
- **Rule setup page**: local HTTP configuration interface for editing, testing, importing, exporting, and saving rules.

## Phased implementation rule

An implementation agent MUST complete all tasks in the current phase before starting the next phase. At the end of every phase, run the phase checkpoint. If any task is incomplete or any checkpoint item fails, the agent MUST continue implementing the remaining work in the current phase. The agent may move to the next phase only after every task is complete and the checkpoint passes.

## Phase 0 — Documentation rename and terminology cleanup

Goal: make the plan safe to build from by adopting project-owned naming.

Tasks:

1. Rename the planning document to `docs/rule-automation-firmware-plan.md`.
2. Update `docs/README.md` to point to `docs/rule-automation-firmware-plan.md`.
3. Replace legacy keyword-based wording with:
   - `StickS3 local rule automation`
   - `WHEN trigger`
   - `DO action`
   - `trigger source`
   - `action adapter`
   - `rule engine`
   - `rule setup page`
4. State that implementation identifiers must not use legacy external-service-style keywords.
5. Preserve the existing intent: StickS3 firmware, local HTTP configuration page, onboard/HAT/GPIO trigger sources, and BLE/HTTP/HAT/IR actions.

Checkpoint:

- Search the docs for legacy external-service-style keywords.
- Search for obsolete planning filenames in `docs/README.md`.
- Confirm this renamed plan still describes StickS3, trigger sources, actions, web configuration, storage, and acceptance criteria.
- If any legacy keyword remains, continue Phase 0.

## Phase 1 — Core rule model and validation

Goal: define the stable data model before touching hardware adapters.

Tasks:

1. Add the core model files:
   - `main/rule_types.h`
   - `main/rule_types.c`
2. Define fixed limits:
   - `RULE_MAX_RULES`
   - `RULE_MAX_ACTIONS_PER_RULE`
   - `RULE_NAME_MAX`
   - `RULE_SOURCE_KEY_MAX`
   - `RULE_HTTP_URL_MAX`
   - `RULE_HTTP_AUTH_MAX`
3. Define trigger source enum covering:
   - onboard sound RMS/peak/clipping
   - KEY1/KEY2
   - BLE state
   - Wi-Fi state
   - battery/power
   - BMI270 future source
   - M5Stack HAT PIR
   - ENV III temperature/humidity/pressure
   - Ambient Light lux
   - ToF distance
   - NCIR temperature
   - third-party GPIO digital/edge/pulse/frequency
   - ADC voltage through verified path
4. Define action enum covering:
   - BLE message
   - HTTP POST
   - HAT operation
   - IR send
   - local UI
5. Define key structs:
   - `rule_value_t`
   - `trigger_fact_t`
   - `rule_condition_t`
   - `rule_action_t`
   - `automation_rule_t`
   - `automation_config_t`
6. Implement:
   - `rule_source_name()`
   - `rule_action_name()`
   - `rule_value_equal()`
   - `automation_config_set_defaults()`
   - `automation_rule_validate()`
   - `automation_config_validate()`
7. Add safe default rules, disabled by default.

Checkpoint:

- `main/rule_types.h` exists and contains all required enums and structs.
- Validation rejects invalid enum values, overlong strings, zero or invalid cooldowns where disallowed, too many rules, unsupported action counts, invalid URLs, unsupported HAT operations, and unsafe GPIO profiles.
- No action execution or hardware polling exists in this phase.
- If validation is incomplete, continue Phase 1.

## Phase 2 — Rule engine

Goal: evaluate facts deterministically without executing actions directly.

Tasks:

1. Add:
   - `main/rule_engine.h`
   - `main/rule_engine.c`
2. Define:
   - `rule_event_t`
   - `rule_engine_t`
3. Implement:
   - `rule_engine_init()`
   - `rule_engine_replace_config()`
   - `rule_engine_process_fact()`
   - `rule_engine_get_rule_by_id()`
4. Implement internal helpers:
   - source matching
   - comparator evaluation
   - sustain-duration tracking
   - false-to-true transition detection
   - cooldown enforcement
5. Ensure the engine returns `rule_event_t` records and never performs BLE, HTTP, HAT, IR, GPIO, I2C, or UI work directly.

Checkpoint:

- A fact that does not match any rule produces zero events.
- A matching condition fires once on transition.
- A sustained condition does not fire before `sustain_ms`.
- A rule does not refire during `cooldown_ms`.
- A condition must become false before transition-based rules can fire again.
- The engine has no direct action side effects.
- If any semantic is missing, continue Phase 2.

## Phase 3 — Onboard trigger adapters

Goal: connect existing StickS3 sound/button data to normalized trigger facts.

Tasks:

1. Add:
   - `main/trigger_sources.h`
   - `main/trigger_sources.c`
2. Define:
   - `trigger_fact_sink_t`
   - `trigger_adapter_t`
3. Implement:
   - `trigger_adapter_init()`
   - `trigger_emit_sound_facts()`
   - `trigger_emit_button_event()`
4. Map existing sound metrics:
   - RMS dBFS Q8 to `RULE_SOURCE_SOUND_RMS_DBFS`
   - peak dBFS Q8 to `RULE_SOURCE_SOUND_PEAK_DBFS`
   - clipped samples to `RULE_SOURCE_SOUND_CLIPPED`
5. Map existing button events:
   - KEY1 short press
   - KEY2 short press
   - both-short either as two facts or a documented combined source key
6. Wire emitted facts into `rule_engine_process_fact()` but do not dispatch actions yet.

Checkpoint:

- Sound facts are emitted from the existing metrics path.
- Button facts are emitted from the existing button event path.
- Facts include source, value, uptime, and sequence.
- Existing sound-meter behavior remains intact.
- If onboard triggers are not connected, continue Phase 3.

## Phase 4 — Capability registry for HAT and GPIO sources

Goal: make M5Stack HAT sensors and third-party GPIO sensors first-class but capability-gated.

Tasks:

1. Add:
   - `main/capability_registry.h`
   - `main/capability_registry.c`
2. Define capability records for:
   - trigger sources
   - actions
   - HAT adapters
   - GPIO profiles
   - pin conflicts
3. Include planned HAT sensor capabilities:
   - PIR HAT motion
   - ENV III temperature/humidity/pressure
   - Ambient Light lux
   - ToF distance
   - NCIR temperature
   - thermal camera summary values
   - heart-rate facts
   - ADC HAT voltage
4. Include third-party GPIO profiles:
   - digital high/low
   - rising/falling edge
   - debounced contact
   - pulse count
   - frequency
5. Add safe-pin validation:
   - reject LCD pins
   - reject I2C pins in use unless assigned to an I2C sensor profile
   - reject I2S/audio pins
   - reject button pins unless explicitly allowed as buttons
   - reject IR pins if IR action is enabled
   - reject boot/USB/internal-risk pins
6. Implement:
   - `capability_source_supported()`
   - `capability_action_supported()`
   - `capability_gpio_profile_validate()`
   - `capability_hat_supported()`
   - `capability_build_json()`

Checkpoint:

- Unsupported HAT trigger sources are rejected during config validation.
- Unsafe GPIO pins are rejected.
- Pin conflicts are reported before save.
- Capability JSON can be served later by the web page.
- If capability validation is incomplete, continue Phase 4.

## Phase 5 — HAT and GPIO trigger drivers

Goal: add real external trigger sources only behind the capability layer.

Tasks:

1. Add HAT trigger adapter files:
   - `main/trigger_hat.h`
   - `main/trigger_hat.c`
2. Add GPIO trigger adapter files:
   - `main/trigger_gpio.h`
   - `main/trigger_gpio.c`
3. Implement PIR HAT digital motion first if its GPIO route is verified.
4. Implement Ambient Light HAT only after BH1750 I2C driver behavior is verified.
5. Implement ENV III only after SHT30 and QMP6988 initialization/read behavior is verified.
6. Implement third-party GPIO digital input with debounce and active-high/active-low settings.
7. Defer advanced HATs unless driver timing/memory costs are understood.
8. Emit normalized facts such as:
   - `hat.pir.motion`
   - `hat.env3.temperature_c`
   - `hat.env3.humidity_rh`
   - `hat.env3.pressure_hpa`
   - `hat.light.lux`
   - `gpio.digital.0`
   - `gpio.edge.0`

Checkpoint:

- Each implemented HAT driver has a probe step.
- Missing HAT hardware disables related sources instead of crashing.
- GPIO digital profile emits facts only from safe validated pins.
- I2C address conflicts are detected.
- If any enabled external source lacks validation or probing, continue Phase 5.

## Phase 6 — Action dispatcher

Goal: execute actions asynchronously so triggers and audio sampling are not blocked.

Tasks:

1. Add:
   - `main/action_dispatcher.h`
   - `main/action_dispatcher.c`
2. Define:
   - `action_result_t`
   - `action_job_t`
   - `action_dispatcher_t`
3. Implement:
   - `action_dispatcher_start()`
   - `action_enqueue()`
   - `action_dispatcher_stop()`
4. Add one FreeRTOS worker task to process queued actions.
5. Store last action result for status/UI.
6. Return timeout/error if the queue is full.
7. Do not call HTTP, BLE notify, HAT operation, or IR transmit from the rule engine.

Checkpoint:

- Rule events can be enqueued.
- Full queues fail safely.
- Action execution occurs only from the dispatcher task.
- Last result is recorded.
- If any action path can block trigger processing directly, continue Phase 6.

## Phase 7 — BLE action

Goal: send local rule events over BLE without implying Bluetooth Classic support.

Tasks:

1. Update:
   - `main/ble_sound_level_protocol.h`
   - `main/transport_ble_gatt_pcm.h`
   - `main/transport_ble_gatt_pcm.c`
2. Add a BLE rule-event characteristic or clearly documented reuse path.
3. Define a compact packet:
   - magic
   - version
   - sequence
   - uptime
   - rule id
   - source
   - action
   - measured value
   - fire count
   - truncated rule name
4. Implement:
   - `transport_ble_send_rule_event()`
   - `transport_ble_rule_event_notify_enabled()`
5. Wire `ACTION_BLE_MESSAGE` in the dispatcher.

Checkpoint:

- Existing sound-level BLE telemetry still works.
- Rule-event notify can be enabled independently.
- BLE action returns `not_ready` if no central is connected or notify is disabled.
- No Bluetooth Classic APIs are introduced.
- If BLE rule events are incomplete, continue Phase 7.

## Phase 8 — HTTP POST action and network readiness

Goal: send JSON events over HTTP from the worker task only.

Tasks:

1. Add:
   - `main/action_http.h`
   - `main/action_http.c`
2. Implement:
   - `rule_event_to_json()`
   - `action_http_post_event()`
3. Validate:
   - URL scheme
   - URL length
   - timeout
   - optional bearer token length
4. Use bounded timeout and zero or one retry.
5. Mask bearer tokens in logs and UI output.
6. Return failure for non-2xx responses.
7. Add Wi-Fi readiness checks before attempting POST.

Checkpoint:

- HTTP action runs only from the action dispatcher.
- Invalid URLs are rejected at config-save time.
- Network unavailable returns `not_ready` or equivalent.
- Secrets are not logged.
- If HTTP can block trigger evaluation or accepts invalid config, continue Phase 8.

## Phase 9 — IR send action

Goal: support StickS3 IR output as a bounded, validated action.

Tasks:

1. Add board constants if missing:
   - `BOARD_IR_TX_GPIO`
   - `BOARD_IR_RX_GPIO`
2. Add:
   - `main/action_ir.h`
   - `main/action_ir.c`
3. Implement RMT-based NEC transmit first.
4. Define:
   - protocol
   - carrier frequency
   - address
   - command
   - repeat count
5. Bound repeat count and transmit timeout.
6. Return unsupported for protocols not implemented.

Checkpoint:

- IR action validates protocol and carrier.
- IR send runs only from dispatcher.
- Repeat count is bounded.
- Unsupported protocol is rejected.
- If IR action can run unbounded or from the wrong context, continue Phase 9.

## Phase 10 — HAT operation actions

Goal: allow DO-side HAT operations only through explicit verified capabilities.

Tasks:

1. Add:
   - `main/action_hat.h`
   - `main/action_hat.c`
2. Define a HAT operation capability table.
3. Implement:
   - `hat_operation_supported()`
   - `hat_run_operation()`
4. Start with no enabled HAT operations unless a driver is already verified.
5. Reject raw arbitrary GPIO/I2C writes from rule config.
6. Future entries may include relay, LED, buzzer, servo, DAC, or other actuator HAT operations only after their bus/register behavior is verified.

Checkpoint:

- Unsupported HAT operations are rejected during config validation.
- Raw GPIO/I2C write actions cannot be saved.
- Supported operations are listed in capability JSON.
- If arbitrary HAT writes are possible, continue Phase 10.

## Phase 11 — Config storage

Goal: persist validated rule configuration with versioning.

Tasks:

1. Add:
   - `main/rule_config_store.h`
   - `main/rule_config_store.c`
2. Store config in NVS with:
   - namespace `rule_auto`
   - key `config`
   - schema version
3. Implement:
   - `rule_config_store_open()`
   - `rule_config_store_load()`
   - `rule_config_store_save()`
   - `rule_config_store_close()`
4. On missing config, use defaults.
5. On invalid config, fail closed to safe defaults and log warning.
6. Add migration hook for future schema changes.

Checkpoint:

- Default config loads when NVS key is missing.
- Invalid stored config does not crash boot.
- Save refuses invalid config.
- Saved config reloads after reboot.
- If persistence can store invalid rules, continue Phase 11.

## Phase 12 — HTTP rule setup page

Goal: provide the local web interface for configuring and testing rules.

Tasks:

1. Add:
   - `main/rule_web.h`
   - `main/rule_web.c`
2. Use ESP-IDF HTTP server.
3. Implement endpoints:
   - `GET /`
   - `GET /api/config`
   - `POST /api/config`
   - `GET /api/capabilities`
   - `GET /api/status`
   - `POST /api/rules/test`
   - `POST /api/gpio/test`
   - `POST /api/hat/probe`
4. The page must expose:
   - trigger source picker
   - condition editor
   - action editor
   - HAT capability status
   - GPIO safety/conflict status
   - import/export JSON
   - test buttons
5. Mask secrets in exported/displayed config.
6. Save flow must validate before writing NVS or replacing runtime config.

Checkpoint:

- User can configure a sound rule.
- User can configure a button rule.
- User can configure supported HAT/GPIO rules only when capabilities pass.
- Invalid config is rejected with a useful error.
- Secrets are masked.
- If the page can save unsupported HAT/GPIO rules, continue Phase 12.

## Phase 13 — Main runtime integration

Goal: connect all modules while preserving existing sound-meter firmware behavior.

Tasks:

1. Update:
   - `main/main.c`
   - `main/CMakeLists.txt`
   - `main/Kconfig.projbuild` if feature flags are needed
2. Add runtime state for:
   - config store
   - rule engine
   - trigger adapters
   - action dispatcher
   - web server
   - IR runtime
   - locks/mutexes
3. Startup order:
   - initialize NVS
   - load config
   - initialize rule engine
   - initialize trigger adapters
   - initialize capabilities
   - start action dispatcher
   - initialize IR if enabled
   - start networking/setup mode
   - start web server
4. Connect sound metrics to trigger facts.
5. Connect button events to trigger facts.
6. Connect HAT/GPIO polling if enabled.
7. On each event, enqueue configured actions.

Checkpoint:

- Existing sound-meter mode still works.
- Existing BLE sound telemetry still works.
- Rule engine receives onboard facts.
- Action dispatcher receives events.
- Web config can replace runtime config without reboot.
- If existing behavior regresses or modules are not wired, continue Phase 13.

## Phase 14 — Tests and hardware smoke validation

Goal: prove the phase implementation works before considering the first version complete.

Tasks:

1. Add host-test coverage for:
   - config validation
   - comparator behavior
   - transition firing
   - sustain duration
   - cooldown
   - unsupported HAT rejection
   - unsafe GPIO rejection
2. Add firmware smoke checklist:
   - boot
   - sound telemetry
   - web page
   - config save/reload
   - BLE rule event
   - HTTP POST with test endpoint
   - IR test command
   - GPIO digital trigger
   - supported HAT probe if hardware is present
3. Document any hardware not available during validation.

Checkpoint:

- All host tests pass.
- Firmware builds.
- Smoke checklist is completed or explicitly marked unavailable per hardware item.
- No phase has an unchecked or failed checkpoint.
- If any required test/check fails, continue Phase 14.
