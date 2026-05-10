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
compile test_pcm_debug_ring -I"${ROOT}/main" \
  "${ROOT}/main/pcm_debug_ring.c" "${ROOT}/tests/host/test_pcm_debug_ring.c"
compile test_ble_sound_protocol -I"${ROOT}/main" \
  "${ROOT}/tests/host/test_ble_sound_protocol.c"
compile test_es8311_sequence -I"${FAKE_INC}" -I"${ROOT}/tests/host/fakes" -I"${ROOT}/main" \
  "${ROOT}/main/es8311.c" "${ROOT}/tests/host/fakes/fake_register_bus.c" "${ROOT}/tests/host/test_es8311_sequence.c"
compile test_m5pm1_gpio -I"${FAKE_INC}" -I"${ROOT}/tests/host/fakes" -I"${ROOT}/main" \
  "${ROOT}/main/m5pm1.c" "${ROOT}/tests/host/fakes/fake_register_bus.c" "${ROOT}/tests/host/test_m5pm1_gpio.c"
compile test_board_audio -I"${FAKE_INC}" -I"${ROOT}/tests/host/fakes" -I"${ROOT}/main" \
  "${ROOT}/main/board_audio.c" "${ROOT}/tests/host/fakes/fake_board_deps.c" "${ROOT}/tests/host/test_board_audio.c"
compile test_board_audio_clock -I"${FAKE_INC}" -I"${ROOT}/main" \
  "${ROOT}/main/board_audio_clock.c" "${ROOT}/tests/host/test_board_audio_clock.c"
"${BUILD_DIR}/test_button_state"
"${BUILD_DIR}/test_audio_resample"
"${BUILD_DIR}/test_audio_pipeline"
"${BUILD_DIR}/test_audio_metrics"
"${BUILD_DIR}/test_app_mode"
"${BUILD_DIR}/test_pcm_debug_ring"
"${BUILD_DIR}/test_ble_sound_protocol"
"${BUILD_DIR}/test_es8311_sequence"
"${BUILD_DIR}/test_m5pm1_gpio"
"${BUILD_DIR}/test_board_audio"
"${BUILD_DIR}/test_board_audio_clock"
