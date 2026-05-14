#!/usr/bin/env python3
"""Validate that default StickS3 boot keeps optional audio bring-up fail-closed."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "main" / "main.c"
BOARD_I2S = ROOT / "main" / "board_i2s.c"
ES8311 = ROOT / "main" / "es8311.c"
CMAKE = ROOT / "main" / "CMakeLists.txt"
OPTIONAL_AUDIO_APP_SRCS = (
    "audio_metrics.c",
    "audio_pipeline.c",
    "audio_resample.c",
    "board_audio.c",
    "board_audio_power.c",
    "board_i2s.c",
    "es8311.c",
    "sound_meter.c",
)


def strip_c_comments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    return re.sub(r"//.*", "", source)


def main() -> int:
    errors: list[str] = []
    main_text = strip_c_comments(MAIN.read_text(encoding="utf-8"))
    if "board_audio_init" in main_text:
        errors.append("main.c default boot must not call optional board_audio_init")
    if "BOARD_AUDIO_PROFILE_CAPTURE_ONLY" in main_text:
        errors.append("main.c must not imply default audio capture is wired without a metrics producer")
    if "ESP_ERROR_CHECK(board_audio_init" in main_text:
        errors.append("audio init failures must not reboot-loop the board")
    if "rule_runtime_process_metrics" in main_text:
        errors.append("main.c must not claim sound metrics are produced until audio capture is wired")

    cmake_text = CMAKE.read_text(encoding="utf-8")
    for source in OPTIONAL_AUDIO_APP_SRCS:
        if f'"{source}"' in cmake_text:
            errors.append(f"default app component must not link optional audio source {source}")

    i2s_text = BOARD_I2S.read_text(encoding="utf-8")
    if "&s_rx_handle" not in i2s_text or "s_tx_handle : NULL" not in i2s_text:
        errors.append("board_i2s.c must default to RX-only standard-channel allocation")
    if "BOARD_AUDIO_PROFILE_FULL_DUPLEX" not in i2s_text or "BOARD_I2S_DO_IO" not in i2s_text:
        errors.append("board_i2s.c must make TX explicit only for full-duplex profile")

    es_text = ES8311.read_text(encoding="utf-8")
    if "ES8311_PROFILE_ADC_ONLY" not in es_text:
        errors.append("es8311.c must implement ADC-only profile")
    if "ES8311_SYSTEM12_DAC_DOWN" not in es_text or "es8311_mute(i2c_num, i2c_addr, true)" not in es_text:
        errors.append("ADC-only codec profile must keep DAC powered down/muted")

    if errors:
        print("Audio-safety validation failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1
    print("Audio-safety validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
