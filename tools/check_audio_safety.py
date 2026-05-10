#!/usr/bin/env python3
"""Validate default StickS3 audio safety policy."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "main" / "main.c"
BOARD_I2S = ROOT / "main" / "board_i2s.c"
ES8311 = ROOT / "main" / "es8311.c"
SOUND_METER = ROOT / "main" / "sound_meter.c"


def main() -> int:
    errors: list[str] = []
    main_text = MAIN.read_text(encoding="utf-8")
    if "BOARD_AUDIO_PROFILE_CAPTURE_ONLY" not in main_text:
        errors.append("main.c must use BOARD_AUDIO_PROFILE_CAPTURE_ONLY for default boot")
    if ".require_audio_power_enable = true" not in main_text:
        errors.append("default boot must enable the source-backed M5PM1 L3B audio rail before ES8311 access")
    if ".probe_m5pm1 = false" not in main_text:
        errors.append("default capture-only boot must skip the optional M5PM1 identity probe")
    if "ESP_ERROR_CHECK(board_audio_init" in main_text:
        errors.append("audio init failures must not reboot-loop the board")

    i2s_text = BOARD_I2S.read_text(encoding="utf-8")
    if "&s_rx_handle" not in i2s_text or "s_tx_handle : NULL" not in i2s_text:
        errors.append("board_i2s.c must default to RX-only standard-channel allocation")
    if "BOARD_AUDIO_PROFILE_FULL_DUPLEX" not in i2s_text or "BOARD_I2S_DO_IO" not in i2s_text:
        errors.append("board_i2s.c must make TX explicit only for full-duplex profile")

    sound_meter_text = SOUND_METER.read_text(encoding="utf-8")
    if "board_i2s_read(pcm_samples, s_config.pcm_chunk_bytes" not in sound_meter_text:
        errors.append("sound_meter.c must honor configured PCM chunk size for I2S reads")
    if "s_config.pcm_chunk_bytes -= s_config.pcm_chunk_bytes % sizeof(int16_t)" not in sound_meter_text:
        errors.append("sound_meter.c must align PCM chunk size to complete int16_t samples")

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
