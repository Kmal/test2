#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests"
FAKE_INC="${ROOT}/tests/host/fakes/esp_compat"
mkdir -p "${BUILD_DIR}"
cc -std=c11 -Wall -Wextra -Werror -I"${ROOT}/main" \
  "${ROOT}/main/button_state.c" "${ROOT}/tests/host/test_button_state.c" \
  -o "${BUILD_DIR}/test_button_state"
cc -std=c11 -Wall -Wextra -Werror -I"${ROOT}/main" \
  "${ROOT}/main/audio_resample.c" "${ROOT}/tests/host/test_audio_resample.c" \
  -o "${BUILD_DIR}/test_audio_resample"
cc -std=c11 -Wall -Wextra -Werror -I"${ROOT}/main" \
  "${ROOT}/main/audio_pipeline.c" "${ROOT}/tests/host/test_audio_pipeline.c" \
  -o "${BUILD_DIR}/test_audio_pipeline"
cc -std=c11 -Wall -Wextra -Werror -I"${ROOT}/main" \
  "${ROOT}/main/audio_metrics.c" "${ROOT}/tests/host/test_audio_metrics.c" -lm \
  -o "${BUILD_DIR}/test_audio_metrics"
cc -std=c11 -Wall -Wextra -Werror -I"${ROOT}/main" \
  "${ROOT}/main/app_mode.c" "${ROOT}/tests/host/test_app_mode.c" \
  -o "${BUILD_DIR}/test_app_mode"
cc -std=c11 -Wall -Wextra -Werror -I"${FAKE_INC}" -I"${ROOT}/tests/host/fakes" -I"${ROOT}/main" \
  "${ROOT}/main/es8311.c" "${ROOT}/tests/host/fakes/fake_register_bus.c" "${ROOT}/tests/host/test_es8311_sequence.c" \
  -o "${BUILD_DIR}/test_es8311_sequence"
cc -std=c11 -Wall -Wextra -Werror -I"${FAKE_INC}" -I"${ROOT}/tests/host/fakes" -I"${ROOT}/main" \
  "${ROOT}/main/m5pm1.c" "${ROOT}/tests/host/fakes/fake_register_bus.c" "${ROOT}/tests/host/test_m5pm1_gpio.c" \
  -o "${BUILD_DIR}/test_m5pm1_gpio"
cc -std=c11 -Wall -Wextra -Werror -I"${FAKE_INC}" -I"${ROOT}/tests/host/fakes" -I"${ROOT}/main" \
  "${ROOT}/main/board_audio.c" "${ROOT}/tests/host/fakes/fake_board_deps.c" "${ROOT}/tests/host/test_board_audio.c" \
  -o "${BUILD_DIR}/test_board_audio"
cc -std=c11 -Wall -Wextra -Werror -I"${FAKE_INC}" -I"${ROOT}/main" \
  "${ROOT}/main/board_audio_clock.c" "${ROOT}/tests/host/test_board_audio_clock.c" \
  -o "${BUILD_DIR}/test_board_audio_clock"
"${BUILD_DIR}/test_button_state"
"${BUILD_DIR}/test_audio_resample"
"${BUILD_DIR}/test_audio_pipeline"
"${BUILD_DIR}/test_audio_metrics"
"${BUILD_DIR}/test_app_mode"
"${BUILD_DIR}/test_es8311_sequence"
"${BUILD_DIR}/test_m5pm1_gpio"
"${BUILD_DIR}/test_board_audio"
"${BUILD_DIR}/test_board_audio_clock"
