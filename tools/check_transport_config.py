#!/usr/bin/env python3
"""Validate that StickS3 defaults do not claim Classic Bluetooth HFP support."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SDKCONFIG_DEFAULTS = ROOT / "config" / "sdkconfig.defaults"
KCONFIG = ROOT / "main" / "Kconfig.projbuild"

FORBIDDEN_ENABLED = {
    "CONFIG_BTDM_CTRL_MODE_BR_EDR_ONLY=y": "ESP32-S3 cannot use BR/EDR-only Classic Bluetooth",
    "CONFIG_BT_HFP_CLIENT_ENABLE=y": "HFP client is Classic Bluetooth and is not a StickS3 default",
    "CONFIG_BT_SCO_DATA_PATH_HCI=y": "SCO-over-HCI is only relevant to Classic Bluetooth HFP",
}


def main() -> int:
    errors: list[str] = []
    defaults = SDKCONFIG_DEFAULTS.read_text(encoding="utf-8")
    for needle, reason in FORBIDDEN_ENABLED.items():
        if needle in defaults:
            errors.append(f"{needle} must not be enabled in StickS3 defaults: {reason}")

    kconfig = KCONFIG.read_text(encoding="utf-8") if KCONFIG.exists() else ""
    if "APP_TRANSPORT_NONE" not in kconfig:
        errors.append("main/Kconfig.projbuild must define APP_TRANSPORT_NONE for the deferred transport state")
    if "APP_TRANSPORT_HFP_LEGACY" not in kconfig:
        errors.append("main/Kconfig.projbuild must define APP_TRANSPORT_HFP_LEGACY if legacy HFP code remains")
    if "depends on !IDF_TARGET_ESP32S3" not in kconfig:
        errors.append("legacy HFP Kconfig option must depend on !IDF_TARGET_ESP32S3")

    if errors:
        print("Transport-config validation failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print("Transport-config validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
