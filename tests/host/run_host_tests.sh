#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests"
FAKE_INC="${ROOT}/tests/host/fakes/esp_compat"
CC_BIN="${CC:-cc}"
EXTRA_CFLAGS=()
EXTRA_LDFLAGS=()
if [[ -n "${CFLAGS:-}" ]]; then
  # shellcheck disable=SC2206 # Intentionally honor conventional space-separated CFLAGS.
  EXTRA_CFLAGS=(${CFLAGS})
fi
if [[ -n "${LDFLAGS:-}" ]]; then
  # shellcheck disable=SC2206 # Intentionally honor conventional space-separated LDFLAGS.
  EXTRA_LDFLAGS=(${LDFLAGS})
fi
mkdir -p "${BUILD_DIR}"
compile() {
  local output="$1"
  shift
  "${CC_BIN}" -std=c11 -Wall -Wextra -Werror "${EXTRA_CFLAGS[@]}" "$@" "${EXTRA_LDFLAGS[@]}" -o "${BUILD_DIR}/${output}"
}
compile test_button_state -I"${ROOT}/main" \
  "${ROOT}/main/button_state.c" "${ROOT}/tests/host/test_button_state.c"
compile test_audio_resample -I"${ROOT}/main" \
  "${ROOT}/main/audio_resample.c" "${ROOT}/tests/host/test_audio_resample.c"
compile test_audio_pipeline -I"${ROOT}/main" \
  "${ROOT}/main/audio_pipeline.c" "${ROOT}/tests/host/test_audio_pipeline.c"
compile test_audio_metrics -I"${ROOT}/main" \
  "${ROOT}/main/audio_metrics.c" "${ROOT}/tests/host/test_audio_metrics.c" -lm
compile test_app_mode -I"${ROOT}/main" \
  "${ROOT}/main/app_mode.c" "${ROOT}/tests/host/test_app_mode.c"
compile test_ui_nav -I"${ROOT}/main" \
  "${ROOT}/main/ui_nav.c" "${ROOT}/tests/host/test_ui_nav.c"
compile test_ui_keyboard -I"${ROOT}/main" \
  "${ROOT}/main/ui_keyboard.c" "${ROOT}/tests/host/test_ui_keyboard.c"
compile test_display_text -I"${ROOT}/main" \
  "${ROOT}/main/display_text.c" "${ROOT}/tests/host/test_display_text.c"
compile test_ble_rule_protocol -I"${ROOT}/main" \
  "${ROOT}/tests/host/test_ble_rule_protocol.c"
compile test_es8311_sequence -I"${FAKE_INC}" -I"${ROOT}/tests/host/fakes" -I"${ROOT}/main" \
  "${ROOT}/main/es8311.c" "${ROOT}/tests/host/fakes/fake_register_bus.c" "${ROOT}/tests/host/test_es8311_sequence.c"
compile test_m5pm1_gpio -I"${FAKE_INC}" -I"${ROOT}/tests/host/fakes" -I"${ROOT}/main" \
  "${ROOT}/main/m5pm1.c" "${ROOT}/main/board_power.c" "${ROOT}/tests/host/fakes/fake_register_bus.c" "${ROOT}/tests/host/test_m5pm1_gpio.c"

compile test_m5pm1_power -I"${FAKE_INC}" -I"${ROOT}/tests/host/fakes" -I"${ROOT}/main" \
  "${ROOT}/main/m5pm1.c" "${ROOT}/tests/host/fakes/fake_register_bus.c" "${ROOT}/tests/host/test_m5pm1_power.c"
compile test_board_power -I"${FAKE_INC}" -I"${ROOT}/tests/host/fakes" -I"${ROOT}/main" \
  "${ROOT}/main/m5pm1.c" "${ROOT}/main/board_power.c" "${ROOT}/tests/host/fakes/fake_register_bus.c" "${ROOT}/tests/host/test_board_power.c"
compile test_bmi270_motion -I"${FAKE_INC}" -I"${ROOT}/tests/host/fakes" -I"${ROOT}/main" \
  "${ROOT}/main/bmi270.c" "${ROOT}/tests/host/fakes/fake_register_bus.c" "${ROOT}/tests/host/test_bmi270_motion.c"
compile test_board_adc -I"${FAKE_INC}" -I"${ROOT}/main" \
  "${ROOT}/main/board_adc.c" "${ROOT}/tests/host/test_board_adc.c"
compile test_hardware_fact_service -I"${FAKE_INC}" -I"${ROOT}/tests/host/fakes" -I"${ROOT}/main" \
  "${ROOT}/main/rule_types.c" "${ROOT}/main/capability_registry.c" "${ROOT}/main/trigger_sources.c" "${ROOT}/main/m5pm1.c" "${ROOT}/main/board_power.c" "${ROOT}/main/bmi270.c" "${ROOT}/main/board_adc.c" "${ROOT}/main/hardware_fact_service.c" "${ROOT}/tests/host/fakes/fake_register_bus.c" "${ROOT}/tests/host/test_hardware_fact_service.c"
compile test_board_audio -I"${FAKE_INC}" -I"${ROOT}/tests/host/fakes" -I"${ROOT}/main" \
  "${ROOT}/main/board_audio.c" "${ROOT}/tests/host/fakes/fake_board_deps.c" "${ROOT}/tests/host/test_board_audio.c"
compile test_board_audio_power -I"${FAKE_INC}" -I"${ROOT}/tests/host/fakes" -I"${ROOT}/main" \
  "${ROOT}/main/m5pm1.c" "${ROOT}/main/board_audio_power.c" "${ROOT}/tests/host/fakes/fake_register_bus.c" "${ROOT}/tests/host/test_board_audio_power.c"
compile test_board_audio_clock -I"${FAKE_INC}" -I"${ROOT}/main" \
  "${ROOT}/main/board_audio_clock.c" "${ROOT}/tests/host/test_board_audio_clock.c"
compile test_board_i2s_decode -I"${FAKE_INC}" -I"${ROOT}/main" \
  "${ROOT}/main/board_i2s.c" "${ROOT}/tests/host/test_board_i2s_decode.c"
compile test_sound_level_service -DSOUND_LEVEL_SERVICE_ENABLE_TEST_HOOKS -I"${FAKE_INC}" -I"${ROOT}/main" \
  "${ROOT}/main/audio_metrics.c" "${ROOT}/main/sound_level_service.c" "${ROOT}/tests/host/test_sound_level_service.c" -lm
compile test_app_sound_level_demand -I"${FAKE_INC}" -I"${ROOT}/main" \
  "${ROOT}/main/rule_types.c" "${ROOT}/main/capability_registry.c" "${ROOT}/main/app_sound_level_demand.c" "${ROOT}/tests/host/test_app_sound_level_demand.c"
compile test_rule_types -I"${FAKE_INC}" -I"${ROOT}/main" \
  "${ROOT}/main/rule_types.c" "${ROOT}/main/capability_registry.c" "${ROOT}/tests/host/test_rule_types.c"
compile test_capability_sound_enabled -DCONFIG_APP_SOUND_LEVEL_TRIGGERS=1 -I"${FAKE_INC}" -I"${ROOT}/main" \
  "${ROOT}/main/rule_types.c" "${ROOT}/main/capability_registry.c" "${ROOT}/tests/host/test_capability_sound_enabled.c"
compile test_capability_hardware_enabled -DCONFIG_APP_POWER_FACTS=1 -DCONFIG_APP_BMI270_FACTS=1 -DCONFIG_APP_ADC_FACTS=1 -I"${FAKE_INC}" -I"${ROOT}/main" \
  "${ROOT}/main/rule_types.c" "${ROOT}/main/capability_registry.c" "${ROOT}/tests/host/test_capability_hardware.c"
compile test_capability_hardware_disabled -DCONFIG_APP_POWER_FACTS=0 -DCONFIG_APP_BMI270_FACTS=0 -DCONFIG_APP_ADC_FACTS=0 -I"${FAKE_INC}" -I"${ROOT}/main" \
  "${ROOT}/main/rule_types.c" "${ROOT}/main/capability_registry.c" "${ROOT}/tests/host/test_capability_hardware.c"
compile test_capability_sound_disabled -DCONFIG_APP_SOUND_LEVEL_TRIGGERS=0 -I"${FAKE_INC}" -I"${ROOT}/main" \
  "${ROOT}/main/rule_types.c" "${ROOT}/main/capability_registry.c" "${ROOT}/tests/host/test_capability_sound_disabled.c"
compile test_rule_engine -I"${FAKE_INC}" -I"${ROOT}/main" \
  "${ROOT}/main/rule_types.c" "${ROOT}/main/capability_registry.c" "${ROOT}/main/rule_engine.c" "${ROOT}/tests/host/test_rule_engine.c"
compile test_trigger_sources -I"${ROOT}/main" \
  "${ROOT}/main/trigger_sources.c" "${ROOT}/tests/host/test_trigger_sources.c"
compile test_action_dispatcher -I"${ROOT}/main" \
  "${ROOT}/main/action_dispatcher.c" "${ROOT}/tests/host/test_action_dispatcher.c"
compile test_rule_runtime -I"${FAKE_INC}" -I"${ROOT}/tests/host/fakes" -I"${ROOT}/main" \
  "${ROOT}/main/rule_types.c" "${ROOT}/main/capability_registry.c" "${ROOT}/main/rule_engine.c" "${ROOT}/main/trigger_sources.c" "${ROOT}/main/trigger_gpio.c" "${ROOT}/main/action_dispatcher.c" "${ROOT}/main/m5pm1.c" "${ROOT}/main/board_power.c" "${ROOT}/main/bmi270.c" "${ROOT}/main/board_adc.c" "${ROOT}/main/hardware_fact_service.c" "${ROOT}/tests/host/fakes/fake_register_bus.c" "${ROOT}/main/rule_runtime.c" "${ROOT}/tests/host/test_rule_runtime.c"
compile test_rule_config_store -I"${FAKE_INC}" -I"${ROOT}/main" \
  "${ROOT}/main/rule_types.c" "${ROOT}/main/capability_registry.c" "${ROOT}/main/rule_config_store.c" "${ROOT}/tests/host/test_rule_config_store.c"
compile test_action_modules -I"${FAKE_INC}" -I"${ROOT}/main" \
  "${ROOT}/main/rule_types.c" "${ROOT}/main/capability_registry.c" "${ROOT}/main/action_http.c" "${ROOT}/main/action_ir.c" "${ROOT}/main/action_speaker.c" "${ROOT}/main/action_hat.c" "${ROOT}/tests/host/test_action_modules.c"
compile test_external_triggers_and_web -I"${FAKE_INC}" -I"${ROOT}/main" \
  "${ROOT}/main/rule_types.c" "${ROOT}/main/capability_registry.c" "${ROOT}/main/rule_engine.c" "${ROOT}/main/trigger_sources.c" "${ROOT}/main/action_dispatcher.c" "${ROOT}/main/m5pm1.c" "${ROOT}/main/board_power.c" "${ROOT}/main/bmi270.c" "${ROOT}/main/board_adc.c" "${ROOT}/main/hardware_fact_service.c" "${ROOT}/tests/host/fakes/fake_register_bus.c" "${ROOT}/main/rule_runtime.c" "${ROOT}/main/rule_config_store.c" "${ROOT}/main/action_http.c" "${ROOT}/main/app_wifi.c" "${ROOT}/main/app_time.c" "${ROOT}/main/rule_web.c" "${ROOT}/main/trigger_gpio.c" "${ROOT}/main/trigger_hat.c" "${ROOT}/tests/host/test_external_triggers_and_web.c"
"${BUILD_DIR}/test_button_state"
"${BUILD_DIR}/test_audio_resample"
"${BUILD_DIR}/test_audio_pipeline"
"${BUILD_DIR}/test_audio_metrics"
"${BUILD_DIR}/test_app_mode"
"${BUILD_DIR}/test_ui_nav"
"${BUILD_DIR}/test_ui_keyboard"
"${BUILD_DIR}/test_display_text"
"${BUILD_DIR}/test_ble_rule_protocol"
"${BUILD_DIR}/test_es8311_sequence"
"${BUILD_DIR}/test_m5pm1_gpio"
"${BUILD_DIR}/test_m5pm1_power"
"${BUILD_DIR}/test_board_power"
"${BUILD_DIR}/test_bmi270_motion"
"${BUILD_DIR}/test_board_adc"
"${BUILD_DIR}/test_hardware_fact_service"
"${BUILD_DIR}/test_board_audio"
"${BUILD_DIR}/test_board_audio_power"
"${BUILD_DIR}/test_board_audio_clock"
"${BUILD_DIR}/test_board_i2s_decode"
"${BUILD_DIR}/test_sound_level_service"
"${BUILD_DIR}/test_app_sound_level_demand"
"${BUILD_DIR}/test_rule_types"
"${BUILD_DIR}/test_capability_sound_enabled"
"${BUILD_DIR}/test_capability_hardware_enabled"
"${BUILD_DIR}/test_capability_hardware_disabled"
"${BUILD_DIR}/test_capability_sound_disabled"
"${BUILD_DIR}/test_rule_engine"
"${BUILD_DIR}/test_trigger_sources"
"${BUILD_DIR}/test_action_dispatcher"
"${BUILD_DIR}/test_rule_runtime"
"${BUILD_DIR}/test_rule_config_store"
"${BUILD_DIR}/test_action_modules"
"${BUILD_DIR}/test_external_triggers_and_web"
