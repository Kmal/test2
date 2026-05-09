#!/usr/bin/env python3
"""Validate StickS3 board constants against the checked-in hardware manifest."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "main" / "board_sticks3.h"
MANIFEST = ROOT / "docs" / "hardware" / "sticks3.board.json"

DEFINE_RE = re.compile(r"^\s*#define\s+(BOARD_[A-Z0-9_]+)\s+([^\s/]+)")
GPIO_NUM_RE = re.compile(r"GPIO_NUM_(\d+)$")


def parse_value(raw: str) -> int | str:
    raw = raw.strip()
    gpio_match = GPIO_NUM_RE.fullmatch(raw)
    if gpio_match:
        return int(gpio_match.group(1))
    if raw.startswith("0x") or raw.startswith("0X"):
        return int(raw, 16)
    if raw.isdigit():
        return int(raw)
    return raw


def load_defines() -> dict[str, int | str]:
    defines: dict[str, int | str] = {}
    for line in HEADER.read_text(encoding="utf-8").splitlines():
        match = DEFINE_RE.match(line)
        if match:
            defines[match.group(1)] = parse_value(match.group(2))
    return defines


def require(defines: dict[str, int | str], name: str, expected: int | str) -> list[str]:
    actual = defines.get(name)
    if actual != expected:
        return [f"{name}: expected {expected!r}, found {actual!r}"]
    return []


def main() -> int:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    defines = load_defines()
    errors: list[str] = []

    pins = manifest["i2s"]["pins"]
    errors += require(defines, "BOARD_I2S_MCLK_IO", pins["mclk"])
    errors += require(defines, "BOARD_I2S_DO_IO", pins["dout"])
    errors += require(defines, "BOARD_I2S_BCK_IO", pins["bclk"])
    errors += require(defines, "BOARD_I2S_WS_IO", pins["lrck"])
    errors += require(defines, "BOARD_I2S_DI_IO", pins["din"])
    errors += require(defines, "BOARD_I2C_SDA_IO", manifest["i2c"]["sda"])
    errors += require(defines, "BOARD_I2C_SCL_IO", manifest["i2c"]["scl"])
    errors += require(defines, "BOARD_ES8311_ADDR", manifest["i2c"]["devices"]["es8311"])
    errors += require(defines, "BOARD_BMI270_ADDR", manifest["i2c"]["devices"]["bmi270"])
    errors += require(defines, "BOARD_M5PM1_ADDR", manifest["i2c"]["devices"]["m5pm1"])
    errors += require(defines, "BOARD_BUTTON_KEY1_GPIO", manifest["buttons"]["key1"])
    errors += require(defines, "BOARD_BUTTON_KEY2_GPIO", manifest["buttons"]["key2"])

    forbidden = set(manifest["buttons"]["forbidden"])
    for name, value in defines.items():
        if name.startswith("BOARD_BUTTON_") and name.endswith("_GPIO") and isinstance(value, int):
            if value in forbidden:
                errors.append(f"{name}: GPIO{value} is forbidden for StickS3 buttons")

    if parse_value("GPIO_NUM_39") != 39:
        errors.append("internal regression: GPIO_NUM_39 did not parse as 39")

    if errors:
        print("Board-map validation failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print("Board-map validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
