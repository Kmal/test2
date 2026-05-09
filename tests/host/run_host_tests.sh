#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests"
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
"${BUILD_DIR}/test_button_state"
"${BUILD_DIR}/test_audio_resample"
"${BUILD_DIR}/test_audio_pipeline"
