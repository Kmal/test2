#!/usr/bin/env python3
"""Validate no-transport StickS3 audio safety policy."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "main" / "main.c"
BOARD_I2S = ROOT / "main" / "board_i2s.c"
ES8311 = ROOT / "main" / "es8311.c"


def main() -> int:
    errors: list[str] = []
    main_text = MAIN.read_text(encoding="utf-8")
    if "BOARD_AUDIO_PROFILE_CAPTURE_ONLY" not in main_text:
        errors.append("main.c must use BOARD_AUDIO_PROFILE_CAPTURE_ONLY for no-transport boot")
    if ".require_audio_power_enable = false" not in main_text:
        errors.append("no-transport boot must not require source-blocked M5PM1 L3B writes")

    i2s_text = BOARD_I2S.read_text(encoding="utf-8")
    if "mode = I2S_MODE_MASTER | I2S_MODE_RX" not in i2s_text:
        errors.append("board_i2s.c must default to RX-only mode")
    if "mode |= I2S_MODE_TX" not in i2s_text:
        errors.append("board_i2s.c must make TX explicit only for full-duplex profile")

    es_text = ES8311.read_text(encoding="utf-8")
    if "ES8311_PROFILE_ADC_ONLY" not in es_text:
        errors.append("es8311.c must implement ADC-only profile")
    if "ES8311_DAC_POWER_DOWN" not in es_text or "es8311_mute(i2c_num, i2c_addr, true)" not in es_text:
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
