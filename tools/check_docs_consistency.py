#!/usr/bin/env python3
"""Validate product docs and StickS3 hardware docs keep their own required facts."""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "docs" / "hardware" / "sticks3.board.json"
README = ROOT / "docs" / "README.md"
HARDWARE_DOC = ROOT / "docs" / "hardware" / "sticks3.md"
DOCS = [README, HARDWARE_DOC]


def main() -> int:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    required_by_doc = {
        README: [
            "custom BLE rule-event and local automation device",
            "ESP32-S3 does not support Bluetooth Classic",
            "Sound-level triggers",
            "Web UI sound telemetry",
            "PCM streaming endpoint",
            "No BLE, Wi-Fi, USB, or debug endpoint streams raw microphone PCM",
            "Raw PCM streaming would be a transport/service capability",
            "StickS3 has onboard speaker hardware",
            "Hardware reference",
            "docs/hardware/sticks3.md",
        ],
        HARDWARE_DOC: [
            manifest["soc"],
            "ESP32-S3 supports Bluetooth LE, not Bluetooth Classic / BR/EDR",
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
            "Hardware present / ⛔ firmware output disabled",
        ],
    }
    errors: list[str] = []
    for doc in DOCS:
        text = doc.read_text(encoding="utf-8")
        for item in required_by_doc.get(doc, []):
            if item not in text:
                errors.append(f"{doc.relative_to(ROOT)} missing required fact {item!r}")

    root_readme = ROOT / "README.md"
    if not root_readme.exists():
        for doc in DOCS:
            text = doc.read_text(encoding="utf-8")
            if "`README.md`" in text:
                errors.append(f"{doc.relative_to(ROOT)} references missing root README.md; use docs/README.md")

    if errors:
        print("Docs consistency validation failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print("Docs consistency validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
