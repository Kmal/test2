# Current C implementation inventory

This inventory is generated from direct source inspection of `main/*.c`, `main/CMakeLists.txt`, and `tests/host/run_host_tests.sh`. It is intentionally one row per C implementation file so default firmware wiring, conditional transports, helper-only code, and host coverage cannot be inferred from stale comments.

| Source | Default app link | Host test | Code-derived status |
| --- | --- | --- | --- |
| `action_dispatcher.c` | default | yes | Queue-backed rule action dispatcher used by `rule_runtime.c`; BLE, HTTP, IR, local UI actions are dispatched or fail closed as unsupported. |
| `action_hat.c` | default | yes | HAT actions are deliberately unsupported; capability/action dispatch remains fail-closed. |
| `action_http.c` | default | yes | HTTP POST action helper is used by the app HTTP sender when network readiness is true; host tests cover JSON/config readiness behavior. |
| `action_ir.c` | default | yes | NEC IR send action helper is wired through the app IR sender and validates carrier/repeat/timeout bounds. |
| `app_mode.c` | default | yes | Initializes and names the local control app mode used by boot/status state. |
| `app_time.c` | default | yes | Provides timezone storage/formatting and `/api/time` JSON support used by boot and Web UI. |
| `app_wifi.c` | default | yes | Starts station/setup-AP support, Wi-Fi scan/connect/AP/mode APIs, network status JSON, and SNTP sync task. |
| `audio_metrics.c` | default via sound config | yes | Audio level metric/calibration helper source linked by the default `CONFIG_APP_SOUND_LEVEL_TRIGGERS=y` build; consumed by `sound_level_service.c` for live sound rules. |
| `audio_pipeline.c` | helper-only | yes | PCM capture/playback buffer helper source; not linked by the default app component. |
| `audio_resample.c` | helper-only | yes | Audio resampling helper source; not linked by the default app component. |
| `board_audio.c` | default via sound config | yes | Capture-only audio initializer linked by the default `CONFIG_APP_SOUND_LEVEL_TRIGGERS=y` build and called from `app_main()` for sound-level triggers. |
| `board_audio_clock.c` | default via sound config | yes | 16 kHz/12.288 MHz/512 kHz audio clock profile helper linked by default sound-level trigger builds. |
| `board_audio_power.c` | default via sound config | no | M5PM1 L3B audio rail enable wrapper linked by default sound-level trigger builds while preserving LCD M5PM1 behavior. |
| `board_i2c.c` | default | no | ESP-IDF shared I2C bus initializer used by LCD/status UI paths and available to board helpers. |
| `board_i2s.c` | default via sound config | yes | Capture-only/full-duplex I2S driver source linked by default sound-level trigger builds; includes mono `int16_t` decode helper coverage. |
| `button_state.c` | default | yes | Active-low KEY1/KEY2 debouncing and event classification for status UI and automation facts. |
| `capability_registry.c` | default | yes | Central capability gate for supported/disabled sources/actions and safe GPIO profile validation. |
| `display_text.c` | default | yes | LCD text measuring, sanitizing, wrapping/marquee, collision, and glyph rendering support. |
| `es8311.c` | helper-only | yes | Optional ES8311 codec profile driver source; not linked by the default app component. |
| `m5pm1.c` | default | yes | M5PM1 register helper and LCD/L3B GPIO sequence used by LCD power, with bit-preserving host tests. |
| `main.c` | default | no | Firmware entry point wiring NVS, time, network, status UI, Wi-Fi, rule runtime, BLE transport, and default error/idle policy. |
| `register_bus.c` | default | no | ESP-IDF I2C register-bus cache/read/write helper used by M5PM1/codec-style drivers. |
| `rule_config_store.c` | default | yes | NVS-backed automation config load/save/default fallback used by boot, Web UI, and status UI updates. |
| `rule_engine.c` | default | yes | Rule condition evaluation, false-to-true firing, sustain, cooldown, action fan-out, and sequence assignment. |
| `rule_runtime.c` | default | yes | Runtime bridge from button/GPIO/BLE/Wi-Fi/sound facts to rule engine and action dispatcher; the default sound service feeds live metrics through this existing path. |
| `rule_types.c` | default | yes | Rule defaults, validation, source/action names, safe GPIO/capability checks, and binary config serialization. |
| `rule_web.c` | default | yes | On-demand HTTP Web UI/API implementation for config, status, time, Wi-Fi, capabilities, test actions, GPIO, and HAT probe. |
| `status_lcd.c` | default | no | Optional LCD bring-up/render task path behind `CONFIG_APP_STATUS_UI_LCD`; failures are non-fatal. |
| `status_ui.c` | default | no | Status UI task, key polling, launcher/menu integration, toasts, service enablement, and automation config callbacks. |
| `transport_ble_gatt.c` | conditional | no | Custom BLE GATT status/rule-event transport compiled when `CONFIG_APP_TRANSPORT_BLE_GATT_PCM=y`. |
| `transport_hfp_legacy.c` | conditional | no | Quarantined legacy Classic HFP placeholder compiled only for non-ESP32-S3 legacy selection and rejected for StickS3. |
| `trigger_gpio.c` | default | yes | Safe GPIO digital/edge trigger initialization and polling with debounce and source-key generation. |
| `trigger_hat.c` | default | yes | HAT source probe path deliberately returns unsupported. |
| `trigger_sources.c` | default | yes | Emits normalized sound, button, and direct facts into a runtime sink. |
| `ui_keyboard.c` | default | yes | 9-key overlay input model for SSID/password/AP/time fields with static keyboard coverage. |
| `ui_model.c` | default | no | Menu/application model for Wi-Fi, AP, Bluetooth, automation editing, Web UI service state, and time settings. |
| `ui_nav.c` | default | yes | Menu graph/navigation state machine for the status UI. |
| `ui_render.c` | default | no | LCD rendering of status bar, menus, Wi-Fi/AP/BLE/automation/settings screens, toasts, and keyboard overlay. |
