#!/usr/bin/env python3
"""Validate key hardware facts appear in project documentation."""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "docs" / "hardware" / "sticks3.board.json"
DOCS = [ROOT / "README.md", ROOT / "docs" / "hardware" / "sticks3.md"]


def main() -> int:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    required = [
        manifest["soc"],
        "ESP32-S3 does not support Bluetooth Classic",
        "GPIO18",
        "GPIO14",
        "GPIO17",
        "GPIO15",
        "GPIO16",
        "GPIO48",
        "GPIO47",
        "0x18",
        "0x68",
        "0x6e",
        "GPIO11",
        "GPIO12",
        "GPIO39",
        "GPIO40",
        "GPIO45",
        "GPIO41",
        "GPIO21",
        "GPIO38",
        "ST7789P3",
        "135x240",
        "12.288 MHz",
        "16 kHz",
        "capture-only",
        "ESP_ERR_NOT_SUPPORTED",
    ]
    errors: list[str] = []
    for doc in DOCS:
        text = doc.read_text(encoding="utf-8")
        for item in required:
            if item not in text:
                errors.append(f"{doc.relative_to(ROOT)} missing required fact {item!r}")

    if errors:
        print("Docs consistency validation failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print("Docs consistency validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
