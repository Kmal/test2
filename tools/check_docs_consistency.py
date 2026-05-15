#!/usr/bin/env python3
"""Validate product docs and StickS3 hardware docs keep their own required facts."""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HARDWARE_DIR = ROOT / "docs" / "hardware" / "sticks3"
MANIFEST = HARDWARE_DIR / "sticks3.board.json"
README = ROOT / "docs" / "README.md"
MAIN_HARDWARE_DOC = HARDWARE_DIR / "sticks3.md"
HARDWARE_DOCS = sorted(HARDWARE_DIR.glob("sticks3_*.md"))
DOCS = [README, MAIN_HARDWARE_DOC, *HARDWARE_DOCS]

CANONICAL_REFERENCE_URLS = [
    "https://docs.m5stack.com/en/core/StickS3",
    "https://docs.m5stack.com/en/arduino/m5sticks3/program",
    "https://docs.m5stack.com/en/arduino/m5sticks3/battery",
    "https://docs.m5stack.com/en/arduino/m5sticks3/button",
    "https://docs.m5stack.com/en/arduino/m5sticks3/display",
    "https://docs.m5stack.com/en/arduino/m5sticks3/imu",
    "https://docs.m5stack.com/en/arduino/m5sticks3/ir_nec",
    "https://docs.m5stack.com/en/arduino/m5sticks3/mic",
    "https://docs.m5stack.com/en/arduino/m5sticks3/speaker",
    "https://docs.m5stack.com/en/arduino/m5sticks3/wakeup",
    "https://docs.m5stack.com/en/arduino/m5sticks3/m5pm1",
    "https://github.com/m5stack/M5PM1",
    "https://github.com/m5stack/M5Unified",
    "https://github.com/m5stack/M5GFX",
    "https://github.com/m5stack/M5GFX/blob/master/src/M5GFX.cpp",
    "https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150_Stick_S3_PRJ_V0.6_20251111_2025_11_17_16_10_24.pdf",
    "https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/products/atom/Atomic%20Echo%20Base/ES8311.pdf",
    "https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/core/K128%20CoreS3/BMI270.PDF",
    "https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/477/esp32-s3_technical_reference_manual_cn.pdf",
    "https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/bt-architecture/overview.html",
]

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
            "bounded 16 kHz square-tone",
            "StickS3 has onboard ES8311/AW8737 speaker hardware",
            "single-owner mic/speaker pattern",
            "caps configured volume below 75%",
            "Hardware reference",
            "Experimental USB Audio Class firmware path",
            "CONFIG_APP_USB_UAC_DEVICE=n",
            "simultaneous mic+speaker profile is experimental",
            "docs/hardware/sticks3/sticks3.md",
            "docs/hardware/sticks3/",
        ],
    }
    errors: list[str] = []
    expected_hardware_docs = {
        "sticks3_battery.md",
        "sticks3_button.md",
        "sticks3_display.md",
        "sticks3_imu.md",
        "sticks3_ir_nec.md",
        "sticks3_mic.md",
        "sticks3_speaker.md",
        "sticks3_wakeup.md",
        "sticks3_m5pm1.md",
        "sticks3_others.md",
    }
    actual_hardware_docs = {doc.name for doc in HARDWARE_DOCS}
    if actual_hardware_docs != expected_hardware_docs:
        errors.append(
            f"docs/hardware/sticks3 topic files mismatch: expected {sorted(expected_hardware_docs)}, found {sorted(actual_hardware_docs)}"
        )

    main_text = MAIN_HARDWARE_DOC.read_text(encoding="utf-8") if MAIN_HARDWARE_DOC.exists() else ""
    if not MAIN_HARDWARE_DOC.exists():
        errors.append("docs/hardware/sticks3/sticks3.md main hardware entry is missing")
    for topic in sorted(expected_hardware_docs):
        if f"]({topic})" not in main_text:
            errors.append(f"docs/hardware/sticks3/sticks3.md missing link to {topic}")
    if "](sticks3.board.json)" not in main_text:
        errors.append("docs/hardware/sticks3/sticks3.md missing link to sticks3.board.json")

    for doc in [doc for doc in [MAIN_HARDWARE_DOC, *HARDWARE_DOCS] if doc.exists()]:
        text = doc.read_text(encoding="utf-8")
        if "## Sources\n" not in text:
            errors.append(f"{doc.relative_to(ROOT)} missing ## Sources citations section")
        source_section = text.split("## Sources\n", 1)[1] if "## Sources\n" in text else ""
        if "https://" not in source_section:
            errors.append(f"{doc.relative_to(ROOT)} missing external source URL in ## Sources")
        if "../../../" not in source_section and doc.name != "sticks3_others.md":
            errors.append(f"{doc.relative_to(ROOT)} missing in-repo source path in ## Sources")

    hardware_text = "\n".join(doc.read_text(encoding="utf-8") for doc in [MAIN_HARDWARE_DOC, *HARDWARE_DOCS] if doc.exists())
    topic_hardware_text = "\n".join(doc.read_text(encoding="utf-8") for doc in HARDWARE_DOCS)
    readme_text = README.read_text(encoding="utf-8")
    for url in CANONICAL_REFERENCE_URLS:
        if url not in topic_hardware_text:
            errors.append(f"docs/hardware/sticks3 topic docs missing canonical reference URL {url!r}")
        if url not in main_text:
            errors.append(f"docs/hardware/sticks3/sticks3.md missing canonical reference URL {url!r}")
        if url not in readme_text:
            errors.append(f"docs/README.md missing canonical reference URL {url!r}")

    for item in [
        manifest["soc"],
        "ESP32-S3 supports Bluetooth LE, not Bluetooth Classic / BR/EDR",
        "Firmware pin mapping",
        "Signal/function",
        "Firmware constant",
        "BOARD_I2S_MCLK_IO",
        "BOARD_LCD_MOSI_GPIO",
        "BOARD_IR_TX_GPIO",
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
        "playback-only",
        "G14_I2S_DDAC",
        "G16_I2S_DADC",
        "below 75%",
        "`74`",
        "Speaker-action conformance review",
        "not expose generic speaker streaming/playback",
        "Implemented/default available",
        "config/sdkconfig.defaults",
    ]:
        if item not in hardware_text:
            errors.append(f"docs/hardware/sticks3 topic docs missing required fact {item!r}")

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

    stale_doc_phrases = [
        "Speaker output remains disabled as a product feature",
        "speaker-control protocol is implemented",
        "exact M5PM1 command/register sequence for enabling and disabling the amplifier is not implemented",
    ]
    for phrase in stale_doc_phrases:
        if phrase in hardware_text:
            errors.append(f"docs/hardware/sticks3 topic docs contain stale speaker-status phrase {phrase!r}")

    manifest_text = MANIFEST.read_text(encoding="utf-8")
    if "no TX/speaker output" in manifest_text:
        errors.append("docs/hardware/sticks3/sticks3.board.json contains stale speaker-status phrase 'no TX/speaker output'")

    if errors:
        print("Docs consistency validation failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print("Docs consistency validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
