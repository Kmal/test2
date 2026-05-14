#!/usr/bin/env python3
"""Validate that default StickS3 audio capture stays demand-driven and fail-closed."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "main" / "main.c"
BOARD_I2S = ROOT / "main" / "board_i2s.c"
ES8311 = ROOT / "main" / "es8311.c"
CMAKE = ROOT / "main" / "CMakeLists.txt"
SOUND_LEVEL_APP_SRCS = (
    "audio_metrics.c",
    "board_audio.c",
    "board_audio_clock.c",
    "board_audio_power.c",
    "board_i2s.c",
    "es8311.c",
    "sound_level_service.c",
)
FORBIDDEN_DEFAULT_AUDIO_APP_SRCS = (
    "audio_pipeline.c",
    "audio_resample.c",
    "sound_meter.c",
)


def strip_c_comments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    return re.sub(r"//.*", "", source)


def main() -> int:
    errors: list[str] = []
    main_text = strip_c_comments(MAIN.read_text(encoding="utf-8"))
    sound_gate = "if (!app_sound_level_capture_needed(config))"
    audio_init = "board_audio_init(&audio_config)"
    if audio_init not in main_text:
        errors.append("main.c must wire board_audio_init for demand-driven sound triggers")
    elif sound_gate not in main_text or main_text.index(sound_gate) > main_text.index(audio_init):
        errors.append("main.c must gate board_audio_init behind shared sound-capture demand")
    if "BOARD_AUDIO_PROFILE_CAPTURE_ONLY" not in main_text:
        errors.append("main.c must request the capture-only profile for sound triggers")
    if "board_audio_deinit" not in main_text:
        errors.append("main.c must release audio resources after sound trigger service stops")
    if "ESP_ERROR_CHECK(board_audio_init" in main_text:
        errors.append("audio init failures must not reboot-loop the board unless explicitly configured")
    if "rule_runtime_process_metrics" in main_text:
        errors.append("main.c must leave sound metric production inside sound_level_service")
    if "app_sound_level_demand_set_telemetry" not in main_text or "s_sound_level_demand" not in main_text:
        errors.append("main.c must track Web UI telemetry sound-capture demand separately from trigger demand")
    if "no_enabled_sound_rule" in main_text and "if (!app_sound_level_capture_needed(&s_rule_config))" not in main_text:
        errors.append("sound status must report no_enabled_sound_rule only when shared capture demand is inactive")
    if "sound_level_service_start(s_sound_level_service)" not in main_text:
        errors.append("main.c must centralize sound_level_service_start in app_sound_level_sync")
    if main_text.count("sound_level_service_start(") != 1:
        errors.append("main.c must have exactly one sound_level_service_start call for the shared capture service")
    ready_guard = "if (s_sound_level_ready) {\n        return;\n    }"
    if ready_guard not in main_text:
        errors.append("main.c must return early when the shared sound capture service is already ready")
    elif main_text.index(ready_guard) > main_text.index("sound_level_service_start(s_sound_level_service)"):
        errors.append("main.c must check s_sound_level_ready before starting the shared sound capture service")

    cmake_text = CMAKE.read_text(encoding="utf-8")
    sound_block_match = re.search(r"if\(CONFIG_APP_SOUND_LEVEL_TRIGGERS\)(.*?)endif\(\)", cmake_text, flags=re.S)
    if not sound_block_match:
        errors.append("default app component must guard sound sources with CONFIG_APP_SOUND_LEVEL_TRIGGERS")
        sound_block = ""
    else:
        sound_block = sound_block_match.group(1)
    for source in SOUND_LEVEL_APP_SRCS:
        if f'"{source}"' not in sound_block:
            errors.append(f"sound-level config must link required source {source}")
    cmake_without_sound_block = cmake_text.replace(sound_block_match.group(0), "") if sound_block_match else cmake_text
    for source in SOUND_LEVEL_APP_SRCS + FORBIDDEN_DEFAULT_AUDIO_APP_SRCS:
        if f'"{source}"' in cmake_without_sound_block:
            errors.append(f"app component must not link audio source outside sound-level config {source}")
    for source in FORBIDDEN_DEFAULT_AUDIO_APP_SRCS:
        if f'"{source}"' in sound_block:
            errors.append(f"sound-level config must not link unsupported audio source {source}")

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
